#include "predicatedSSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Verifier.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

// Compare two predicates for equality
bool predicatesEqual(SSAPredicate *A, SSAPredicate *B)
{
    if (A->kind != B->kind)
        return false;

    switch (A->kind)
    {
    case SSAPredicate::True:
        return true;

    case SSAPredicate::Condition:
        return A->condition == B->condition;

    case SSAPredicate::Not:
        return predicatesEqual(A->left, B->left);

    case SSAPredicate::And:
    case SSAPredicate::Or:
        return (predicatesEqual(A->left, B->left) &&
                predicatesEqual(A->right, B->right)) ||
               (predicatesEqual(A->left, B->right) &&
                predicatesEqual(A->right, B->left));
    }
    return false;
}

class BlockBuilder
{
private:
    std::unordered_map<SSAPredicate *, llvm::BasicBlock *> predicateBlockMap;
    BasicBlock *entryBlock;
    Function *currentFunction;

public:
    BlockBuilder(llvm::BasicBlock *entry)
        : entryBlock(entry), currentFunction(entry->getParent()) {}

    llvm::BasicBlock *get_or_create_block(SSAPredicate *pred, BasicBlock *insertAfter = nullptr)
    {
        // First, try to find existing block for this predicate
        for (auto &pair : predicateBlockMap)
        {
            if (predicatesEqual(pair.first, pred))
            {
                return pair.second;
            }
        }
        
        // Create new block with proper insertion point
        BasicBlock *newBlock = nullptr;
        if (insertAfter)
        {
            // Insert after the specified block
            newBlock = BasicBlock::Create(entryBlock->getContext(), 
                                         "pred_block", 
                                         currentFunction,
                                         insertAfter->getNextNode());
        }
        else
        {
            // Insert at the end of the function
            newBlock = BasicBlock::Create(entryBlock->getContext(), 
                                         "pred_block", 
                                         currentFunction);
        }
        
        predicateBlockMap[pred] = newBlock;
        return newBlock;
    }
    
    // Helper method to get block without creating new one
    llvm::BasicBlock *get_existing_block(SSAPredicate *pred)
    {
        for (auto &pair : predicateBlockMap)
        {
            if (predicatesEqual(pair.first, pred))
            {
                return pair.second;
            }
        }
        return nullptr;
    }
    
    // Create a block that should come after a specific predecessor
    llvm::BasicBlock *create_successor_block(SSAPredicate *pred, BasicBlock *predecessor)
    {
        BasicBlock *newBlock = BasicBlock::Create(entryBlock->getContext(),
                                                 "succ_block",
                                                 currentFunction);
        predicateBlockMap[pred] = newBlock;
        
        // Create branch from predecessor to new block if predecessor doesn't have terminator
        if (predecessor && predecessor->getTerminator() == nullptr)
        {
            BranchInst::Create(newBlock, predecessor);
        }
        
        return newBlock;
    }
};



SSAPredicate *getControlPredicate(BasicBlock *BB)
{
    // Stub implementation - in real code this would compute control predicates
    return new SSAPredicate{SSAPredicate::True};
}

class SSAPredicatedSSAConverter
{
private:
    llvm::Function &llvmFunc;
    llvm::DominatorTree DT;
    llvm::PostDominatorTree PDT;
    llvm::LoopInfo LI;

    // Cache for computed predicates
    std::unordered_map<llvm::BasicBlock *, SSAPredicate *> predicateCache;

    // Get post-dominance frontier of a block
    std::vector<llvm::BasicBlock *> getPostDominanceFrontier(llvm::BasicBlock *BB)
    {
        std::vector<llvm::BasicBlock *> pdf;

        for (auto pred_iter = llvm::pred_begin(BB), pred_end = llvm::pred_end(BB);
             pred_iter != pred_end; ++pred_iter)
        {
            llvm::BasicBlock *pred = *pred_iter;

            llvm::DomTreeNodeBase<llvm::BasicBlock> *node = PDT.getNode(pred);
            while (node)
            {
                llvm::BasicBlock *candidate = node->getBlock();
                if (!PDT.dominates(candidate, BB))
                {
                    if (PDT.dominates(candidate, pred))
                    {
                        pdf.push_back(candidate);
                    }
                    break;
                }
                node = node->getIDom();
            }
        }

        return pdf;
    }

    // Find the unique successor of BPrime that is post-dominated by BB
    llvm::BasicBlock *getUniqueSuccessorPostDominatedBy(llvm::BasicBlock *BPrime, llvm::BasicBlock *BB)
    {
        for (auto succ_iter = llvm::succ_begin(BPrime), succ_end = llvm::succ_end(BPrime);
             succ_iter != succ_end; ++succ_iter)
        {
            llvm::BasicBlock *succ = *succ_iter;
            if (PDT.dominates(BB, succ))
            {
                return succ;
            }
        }
        return nullptr;
    }

    // Get branch condition as SSAPredicate
    SSAPredicate *getBranchCondition(llvm::BasicBlock *From, llvm::BasicBlock *To)
    {
        llvm::Instruction *terminator = From->getTerminator();

        // In canonical form, should only be BranchInst or Return/Unreachable
        if (llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(terminator))
        {
            if (br->isConditional())
            {
                llvm::Value *cond = br->getCondition();

                // Determine which successor we're taking
                llvm::BasicBlock *trueSucc = br->getSuccessor(0);
                llvm::BasicBlock *falseSucc = br->getSuccessor(1);

                if (To == trueSucc)
                {
                    SSAPredicate *pred = new SSAPredicate();
                    pred->kind = SSAPredicate::Condition;
                    pred->condition = cond;
                    return pred;
                }
                else if (To == falseSucc)
                {
                    SSAPredicate *condPred = new SSAPredicate();
                    condPred->kind = SSAPredicate::Condition;
                    condPred->condition = cond;

                    SSAPredicate *notPred = new SSAPredicate();
                    notPred->kind = SSAPredicate::Not;
                    notPred->left = condPred;
                    return notPred;
                }
            }
        } else {
            errs() << "Unhandled terminator in getBranchCondition: " << *terminator << "\n";
        }

        SSAPredicate *truePred = new SSAPredicate();
        truePred->kind = SSAPredicate::True;
        return truePred;
    }

    // Simplify predicate expressions
    SSAPredicate *simplifyPredicate(SSAPredicate *pred)
    {
        if (!pred)
        {
            SSAPredicate *truePred = new SSAPredicate();
            truePred->kind = SSAPredicate::True;
            return truePred;
        }

        switch (pred->kind)
        {
        case SSAPredicate::And:
        case SSAPredicate::Or:
        {
            pred->left = simplifyPredicate(pred->left);
            pred->right = simplifyPredicate(pred->right);

            if (pred->kind == SSAPredicate::And)
            {
                if (pred->left->kind == SSAPredicate::True)
                {
                    SSAPredicate *result = pred->right;
                    delete pred->left;
                    delete pred;
                    return result;
                }
                if (pred->right->kind == SSAPredicate::True)
                {
                    SSAPredicate *result = pred->left;
                    delete pred->right;
                    delete pred;
                    return result;
                }
            }
            else if (pred->kind == SSAPredicate::Or)
            {
                if (pred->left->kind == SSAPredicate::True ||
                    pred->right->kind == SSAPredicate::True)
                {
                    SSAPredicate *truePred = new SSAPredicate();
                    truePred->kind = SSAPredicate::True;
                    delete pred->left;
                    delete pred->right;
                    delete pred;
                    return truePred;
                }
                if (predicatesEqual(pred->left, pred->right))
                {
                    SSAPredicate *result = pred->left;
                    delete pred;
                    return result;
                }
            }
            break;
        }

        case SSAPredicate::Not:
            pred->left = simplifyPredicate(pred->left);
            if (pred->left->kind == SSAPredicate::Not)
            {
                SSAPredicate *result = pred->left->left;
                delete pred->left;
                delete pred;
                return result;
            }
            break;

        default:
            break;
        }

        return pred;
    }

    // Compute edge predicate cpedge(B1, B2)
    SSAPredicate *computeEdgePredicate(llvm::BasicBlock *B1, llvm::BasicBlock *B2)
    {
        // Check if B1->B2 is a back edge (To dominates From)
        if (DT.dominates(B2, B1))
        {
            SSAPredicate *truePred = new SSAPredicate();
            truePred->kind = SSAPredicate::True;
            return truePred;
        }

        // Get branch condition
        SSAPredicate *edgeCondition = getBranchCondition(B1, B2);

        // Get control predicate for B1
        SSAPredicate *B1Predicate = getControlPredicateImpl(B1);

        // Count predecessors of B2
        unsigned predCount = 0;
        for (auto pred_iter = llvm::pred_begin(B2), pred_end = llvm::pred_end(B2);
             pred_iter != pred_end; ++pred_iter)
        {
            predCount++;
        }

        // If B2 has only one predecessor (which must be B1)
        if (predCount == 1)
        {
            delete edgeCondition;
            return B1Predicate;
        }

        // Check if B1->B2 is loop-exiting
        llvm::Loop *L = LI.getLoopFor(B1);
        bool isLoopExiting = L && L->contains(B1) && (!L->contains(B2) || B2 == nullptr);

        if (isLoopExiting)
        {
            llvm::BasicBlock *preheader = L->getLoopPreheader();
            if (!preheader)
            {
                if (edgeCondition->kind == SSAPredicate::True)
                {
                    delete edgeCondition;
                    return B1Predicate;
                }
                SSAPredicate *andPred = new SSAPredicate();
                andPred->kind = SSAPredicate::And;
                andPred->left = B1Predicate;
                andPred->right = edgeCondition;
                return andPred;
            }

            SSAPredicate *preheaderPredicate = getControlPredicateImpl(preheader);

            // preheaderPredicate ∧ B1Predicate ∧ edgeCondition
            SSAPredicate *and1 = new SSAPredicate();
            and1->kind = SSAPredicate::And;
            and1->left = preheaderPredicate;
            and1->right = B1Predicate;

            SSAPredicate *and2 = new SSAPredicate();
            and2->kind = SSAPredicate::And;
            and2->left = and1;
            and2->right = edgeCondition;

            return and2;
        }

        // Otherwise: B1Predicate ∧ edgeCondition
        if (edgeCondition->kind == SSAPredicate::True)
        {
            delete edgeCondition;
            return B1Predicate;
        }

        SSAPredicate *andPred = new SSAPredicate();
        andPred->kind = SSAPredicate::And;
        andPred->left = B1Predicate;
        andPred->right = edgeCondition;
        return andPred;
    }

    // Main implementation
    SSAPredicate *getControlPredicateImpl(llvm::BasicBlock *BB)
    {
        // Check cache
        auto it = predicateCache.find(BB);
        if (it != predicateCache.end())
        {
            return it->second;
        }

        // Check control-flow equivalence to loop header
        llvm::Loop *L = LI.getLoopFor(BB);
        if (L)
        {
            llvm::BasicBlock *loopHeader = L->getHeader();
            if (DT.dominates(loopHeader, BB) && PDT.dominates(BB, loopHeader))
            {
                SSAPredicate *truePred = new SSAPredicate();
                truePred->kind = SSAPredicate::True;
                predicateCache[BB] = truePred;
                return truePred;
            }
        }

        // Get post-dominance frontier
        auto pdf = getPostDominanceFrontier(BB);

        // If no control dependences, predicate is true
        if (pdf.empty())
        {
            SSAPredicate *truePred = new SSAPredicate();
            truePred->kind = SSAPredicate::True;
            predicateCache[BB] = truePred;
            return truePred;
        }

        // Compute disjunction over all control-dependent blocks
        SSAPredicate *result = nullptr;
        for (llvm::BasicBlock *BPrime : pdf)
        {
            llvm::BasicBlock *succ = getUniqueSuccessorPostDominatedBy(BPrime, BB);
            if (!succ)
                continue;

            SSAPredicate *edgePred = computeEdgePredicate(BPrime, succ);

            if (!result)
            {
                result = edgePred;
            }
            else
            {
                SSAPredicate *orPred = new SSAPredicate();
                orPred->kind = SSAPredicate::Or;
                orPred->left = result;
                orPred->right = edgePred;
                result = orPred;
            }
        }

        // Simplify the predicate
        result = simplifyPredicate(result);

        // Cache the result
        predicateCache[BB] = result;
        return result;
    }

    std::unordered_map<Value *, SSAValue> valueMap;
    std::vector<Item> processBasicBlock(BasicBlock *BB, SSAPredicate *pred)
    {
        std::vector<Item> items;
        for (auto &I : *BB)
        {
            Item item;
            item.content = &I;
            item.Predicate = pred;
            items.push_back(item);
            valueMap[&I] = &I;
        }
        return items;
    }

    SSALoop *processLoop(Loop *L)
    {
        SSALoop *ssaLoop = new SSALoop();

        BasicBlock *header = L->getHeader();
        BasicBlock *preheader = L->getLoopPreheader();
        BasicBlock *latch = L->getLoopLatch();

        if (!preheader || !latch)
        {
            return ssaLoop;
        }

        for (auto &I : *header)
        {
            if (PHINode *phi = dyn_cast<PHINode>(&I))
            {
                Value *initValue = nullptr;
                Value *recValue = nullptr;

                for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
                {
                    BasicBlock *incomingBB = phi->getIncomingBlock(i);
                    if (incomingBB == preheader)
                    {
                        initValue = phi->getIncomingValue(i);
                    }
                    else if (incomingBB == latch)
                    {
                        recValue = phi->getIncomingValue(i);
                    }
                }

                if (initValue && recValue)
                {
                    SSAMuNode *muNode = new SSAMuNode();

                    if (valueMap.find(initValue) != valueMap.end())
                    {
                        muNode->init = valueMap[initValue];
                    }
                    else
                    {
                        muNode->init = initValue;
                    }

                    if (valueMap.find(recValue) != valueMap.end())
                    {
                        muNode->rec = valueMap[recValue];
                    }
                    else
                    {
                        muNode->rec = recValue;
                    }

                    SSALoop::MuBinding binding;
                    binding.variable = phi->getName().str();
                    binding.muNode = muNode;
                    ssaLoop->muBindings.push_back(binding);

                    valueMap[phi] = muNode;
                }
            }
        }

        if (auto *branch = dyn_cast<BranchInst>(latch->getTerminator()))
        {
            if (branch->isConditional())
            {
                SSAPredicate *condPred = new SSAPredicate{SSAPredicate::Condition};
                condPred->condition = branch->getCondition();
                ssaLoop->whileCondition = condPred;
            }
        }

        std::vector<BasicBlock *> loopBlocks;
        for (auto *BB : L->blocks())
        {
            if (BB != header)
            {
                loopBlocks.push_back(BB);
            }
        }

        for (auto *BB : loopBlocks)
        {
            if (LI.getLoopFor(BB) == L && LI.isLoopHeader(BB))
            {
                for (auto *subLoop : LI)
                {
                    if (subLoop->getHeader() == BB && subLoop->getParentLoop() == L)
                    {
                        SSALoop *nestedLoop = processLoop(subLoop);
                        Item loopItem;
                        loopItem.content = nestedLoop;
                        loopItem.Predicate = getControlPredicate(BB);
                        ssaLoop->bodyItems.push_back(loopItem);

                        std::unordered_set<BasicBlock *> nestedBlocks(
                            subLoop->block_begin(), subLoop->block_end());
                        auto it = std::remove_if(loopBlocks.begin(), loopBlocks.end(),
                                                 [&](BasicBlock *b)
                                                 { return nestedBlocks.count(b); });
                        loopBlocks.erase(it, loopBlocks.end());
                        break;
                    }
                }
            }
            else
            {
                SSAPredicate *blockPred = getControlPredicate(BB);
                auto items = processBasicBlock(BB, blockPred);
                ssaLoop->bodyItems.insert(ssaLoop->bodyItems.end(), items.begin(), items.end());
            }
        }

        return ssaLoop;
    }

public:
    SSAPredicatedSSAConverter(Function &F)
        : llvmFunc(F), DT(F), PDT(F), LI(DT)
    {
    }
    SSAFunction *convertToPredicatedSSA()
    {
        SSAFunction *ssaFunc = new SSAFunction();
        std::vector<BasicBlock *> topLevelBlocks;
        std::unordered_set<BasicBlock *> skips;
        for (auto &BB : llvmFunc)
        {
            topLevelBlocks.push_back(&BB);
        }

        for (auto *BB : topLevelBlocks)
        {
            if (skips.count(BB))
                continue;
            if (LI.isLoopHeader(BB))
            {
                for (auto *L : LI)
                {
                    if (L->getHeader() == BB && !L->getParentLoop())
                    {
                        SSALoop *loop = processLoop(L);
                        Item loopItem;
                        loopItem.content = loop;
                        loopItem.Predicate = getControlPredicate(BB);
                        ssaFunc->items.push_back(loopItem);

                        // Skip blocks that are part of this loop
                        for (auto BB : L->blocks())
                        {
                            skips.insert(BB);
                        }
                        break;
                    }
                }
            }
            else
            {
                SSAPredicate *blockPred = getControlPredicate(BB);
                auto items = processBasicBlock(BB, blockPred);
                ssaFunc->items.insert(ssaFunc->items.end(), items.begin(), items.end());
            }
        }

        return ssaFunc;
    }

    void clearPhiNode(PHINode *phi, BlockBuilder &bb)
    {
        // STUB implementation - in real code this would eliminate the PHI node
    }

    void eliminatePhiNodes(std::variant<SSAFunction *, SSALoop *> function_or_loop, BlockBuilder &bb)
    {
        if (auto loop = std::get_if<SSALoop *>(&function_or_loop))
        {

            for (auto &item : (*loop)->bodyItems)
            {
                if (auto instr = std::get_if<Instruction *>(&item.content))
                    if(auto phi = dyn_cast<PHINode>(*instr)) {
                        clearPhiNode(phi, bb);
                    }
            }
        }
        else if (auto function = std::get_if<SSAFunction *>(&function_or_loop))
        {
            for (auto &item : (*function)->items)
            {
                if (auto instr = std::get_if<Instruction *>(&item.content))
                    if(auto phi = dyn_cast<PHINode>(*instr)) {
                        clearPhiNode(phi, bb);
                    }
            }
        }
    }

    void restore_ssa(std::variant<SSAFunction *, SSALoop *> function_or_loop, BlockBuilder &bb)
    {
        // Stub implementation - in real code this would restore SSA form
    }

    PHINode* eliminateMu(SSALoop::MuBinding *muNode, BasicBlock* header, BasicBlock* entry, BasicBlock* latch)
    {
        PHINode *node = PHINode::Create(muNode->muNode->type, 2, muNode->variable, header);

        if(auto init = std::get_if<llvm::Value*>(&muNode->muNode->init))
        {
            node->addIncoming(*init, entry);
        }
        else if(auto initMu = std::get_if<SSAMuNode*>(&muNode->muNode->init))
        {
            PHINode* initPhi = eliminateMu(new SSALoop::MuBinding{muNode->variable + "_init", *initMu}, header, entry, latch);
            node->addIncoming(initPhi, entry);
        }
        if(auto rec = std::get_if<llvm::Value*>(&muNode->muNode->rec))
        {
            node->addIncoming(*rec, latch);
        }
        else if(auto recMu = std::get_if<SSAMuNode*>(&muNode->muNode->rec))
        {
            PHINode* recPhi = eliminateMu(new SSALoop::MuBinding{muNode->variable + "_rec", *recMu}, header, entry, latch);
            node->addIncoming(recPhi, latch);
        }
        return node;
    }

   void lowerToIR(std::variant<SSAFunction *, SSALoop *> function_or_loop, 
               BasicBlock *entry, LLVMContext &ctx)
{
    BlockBuilder blockBuilder = BlockBuilder(entry);
    eliminatePhiNodes(function_or_loop, blockBuilder);

    if (auto loop = std::get_if<SSALoop *>(&function_or_loop))
    {
        BasicBlock *header = BasicBlock::Create(ctx, "loop_header", entry->getParent());
        BasicBlock *latch = BasicBlock::Create(ctx, "loop_latch", entry->getParent());
        BasicBlock *exit = BasicBlock::Create(ctx, "loop_exit", entry->getParent());
        
        // Connect entry to header
        if (entry->getTerminator() == nullptr)
        {
            BranchInst::Create(header, entry);
        }
        
        // Process loop body items
        for (size_t i = 0; i < (*loop)->bodyItems.size(); ++i)
        {
            auto &item = (*loop)->bodyItems[i];
            BasicBlock *block = nullptr;
            
            // Determine predecessor for this block
            BasicBlock *pred = nullptr;
            if (i == 0)
            {
                pred = header;  // First block after header
            }
            else if (i > 0)
            {
                // Get block from previous item
                auto &prevItem = (*loop)->bodyItems[i-1];
                pred = blockBuilder.get_existing_block(prevItem.Predicate);
            }
            
            // Create or get block for current item
            if (pred)
            {
                block = blockBuilder.create_successor_block(item.Predicate, pred);
            }
            else
            {
                block = blockBuilder.get_or_create_block(item.Predicate);
            }
            
            if (auto innerLoop = std::get_if<SSALoop *>(&item.content))
            {
                lowerToIR(*innerLoop, block, ctx);
            }
            else if (auto instr = std::get_if<Instruction *>(&item.content))
            {
                Instruction *clone = (*instr)->clone();
                errs() << "Inserting instruction: " << *clone << "\n";
                if (Instruction *term = block->getTerminator())
                {
                    // Insert before the existing terminator
                    clone->insertBefore(term);
                }
                else
                {
                    // Append to the end of the block
                    block->getInstList().push_back(clone);
                }
            }
        }
        
        // Connect last block back to latch
        if (!(*loop)->bodyItems.empty())
        {
            auto &lastItem = (*loop)->bodyItems.back();
            BasicBlock *lastBlock = blockBuilder.get_existing_block(lastItem.Predicate);
            if (lastBlock && lastBlock->getTerminator() == nullptr)
            {
                BranchInst::Create(latch, lastBlock);
            }
        }
        
        // Create loop control flow
        // header -> first block
        // last block -> latch
        // latch -> header or exit based on condition
        
        // Create loop condition (simplified - you'll need actual condition logic)
        BranchInst::Create(header, exit, /* condition */ nullptr, latch);
        
        // Connect header to exit for loop initialization
        BranchInst::Create(exit, header);
    }
    else if (auto function = std::get_if<SSAFunction *>(&function_or_loop))
    {
        BasicBlock *currentBlock = entry;
        
        for (size_t i = 0; i < (*function)->items.size(); ++i)
        {
            auto &item = (*function)->items[i];
            BasicBlock *block = nullptr;
            
            if (i == 0)
            {
                // First block uses entry
                block = blockBuilder.get_or_create_block(item.Predicate);
                if (currentBlock->getTerminator() == nullptr)
                {
                    BranchInst::Create(block, currentBlock);
                }
            }
            else
            {
                // Subsequent blocks chain from previous
                auto &prevItem = (*function)->items[i-1];
                BasicBlock *prevBlock = blockBuilder.get_existing_block(prevItem.Predicate);
                block = blockBuilder.create_successor_block(item.Predicate, prevBlock);
            }
            
            currentBlock = block;
            
            if (auto loop = std::get_if<SSALoop *>(&item.content))
            {
                lowerToIR(*loop, block, ctx);
            }
            else if (auto instr = std::get_if<Instruction *>(&item.content))
            {
                Instruction *clone = (*instr)->clone();
                errs() << "Inserting instruction: " << *clone << "\n";
                if (Instruction *term = block->getTerminator())
                {
                    // Insert before the existing terminator
                    clone->insertBefore(term);
                }
                else
                {
                    // Append to the end of the block
                    block->getInstList().push_back(clone);
                }
            }
        }
    }
    restore_ssa(function_or_loop, blockBuilder);
}
};

// Main entry point
SSAFunction *convertToPredicatedSSA(llvm::Function &llvmFunc)
{
    SSAPredicatedSSAConverter converter(llvmFunc);
    return converter.convertToPredicatedSSA();
}

void lowerToIR(SSAFunction *function, llvm::Function &llvmFunc)
{
    SSAPredicatedSSAConverter converter(llvmFunc);
    BasicBlock* newEntry = BasicBlock::Create(llvmFunc.getContext());
    converter.lowerToIR(function, newEntry, llvmFunc.getContext());
    newEntry->insertInto(&llvmFunc, &llvmFunc.getEntryBlock());
    std::vector<BasicBlock *> OldBlocks;
    for (auto &BB : llvmFunc)
        if (&BB != newEntry)
            OldBlocks.push_back(&BB);

    for (auto *BB : OldBlocks)
        BB->dropAllReferences();
    for (auto *BB : OldBlocks)
        BB->eraseFromParent();
    verifyFunction(llvmFunc);
}
