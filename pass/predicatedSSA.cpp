#include "predicatedSSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/Verifier.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

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
    //std::unordered_map<SSAPredicate *, BasicBlock*> predicateBlockMap;
    BasicBlock *entryBlock;
    BasicBlock * trueBlock;
    SSAPredicate* activePredicate;
    BasicBlock * last;
    Function *currentFunction;
    ValueToValueMapTy* VMap;

public:
    BlockBuilder(llvm::BasicBlock *entry, ValueToValueMapTy* vmap)
        : entryBlock(entry), last(entry), trueBlock(entry), currentFunction(entry->getParent()), VMap(vmap) {
            activePredicate = new SSAPredicate();
            activePredicate->kind = SSAPredicate::True;
        }

    BasicBlock *get_block(SSAPredicate *pred) {
        // Simplified version without caching for expediency
        if (predicatesEqual(pred, activePredicate)) {
            return last;
        }
        if (pred->kind == SSAPredicate::True) {
            return last;
        }
        BasicBlock* newBlock = BasicBlock::Create(entryBlock->getContext(), 
                                            "pred_block", 
                                            currentFunction);
        BasicBlock* newTrue = BasicBlock::Create(entryBlock->getContext(), 
                                            "true_block", 
                                            currentFunction);

        switch(pred->kind) {
            case SSAPredicate::Condition: {
                Value *remapped_cond;
                auto it = VMap->find(pred->condition);
                if (it != VMap->end()) {
                    remapped_cond = (Value*)it->second;
                } else {
                    remapped_cond = pred->condition;
                }
                BasicBlock* newBlock = BasicBlock::Create(entryBlock->getContext(), 
                                            "pred_block", 
                                            currentFunction);
                BasicBlock* newTrue = BasicBlock::Create(entryBlock->getContext(), 
                                            "true_block", 
                                            currentFunction);
                last = newBlock;
                trueBlock = newTrue;
                return last;
                break;
            }
            case SSAPredicate::Not: {
                SSAPredicate* inner = pred->left;
                get_block(inner);
                return trueBlock;
                break;
            }
            case SSAPredicate::And: {
                BasicBlock* leftBlock = get_block(pred->left);
                BasicBlock* rightBlock = BasicBlock::Create(entryBlock->getContext(), 
                                                    "and_right", 
                                                    currentFunction);
                last = newBlock;
                return newBlock;
                break;
            }
            case SSAPredicate::Or: {
                BasicBlock* leftBlock = get_block(pred->left);
                BasicBlock* rightBlock = get_block(pred->right);
                last = newBlock;
                return newBlock;
                break;
            }
            default:
                break;
        }
        
        return last;
    }

    void append(BasicBlock* block, SSAPredicate* pred) {
        BasicBlock* predBlock = get_block(pred);
        BranchInst::Create(block, predBlock);
        last = block;
    }

    BasicBlock* getTrueBlock() {
        return trueBlock;
    }

    void seal_off(BasicBlock* target) {
        for (auto &block : *currentFunction) {
                if (!block.getTerminator()) {
                    BranchInst::Create(target, &block);
                }
            }
    }
};

class SSAPredicatedSSAConverter
{
private:
    llvm::Function &llvmFunc;
    llvm::DominatorTree DT;
    llvm::PostDominatorTree PDT;
    llvm::LoopInfo LI;
    ValueToValueMapTy VMap;

    std::unordered_map<llvm::BasicBlock *, SSAPredicate *> predicateCache;

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
                    return pred->right;
                }
                if (pred->right->kind == SSAPredicate::True)
                {
                    return pred->left;
                }
            }
            else if (pred->kind == SSAPredicate::Or)
            {
                if (pred->left->kind == SSAPredicate::True ||
                    pred->right->kind == SSAPredicate::True)
                {
                    SSAPredicate *truePred = new SSAPredicate();
                    truePred->kind = SSAPredicate::True;
                    return truePred;
                }
                if (predicatesEqual(pred->left, pred->right))
                {
                    return pred->left;
                }
            }
            break;
        }

        case SSAPredicate::Not:
            pred->left = simplifyPredicate(pred->left);
            if (pred->left->kind == SSAPredicate::Not)
            {
                return pred->left->left;
            }
            break;

        default:
            break;
        }

        return pred;
    }





    SSAPredicate* truth() {
        SSAPredicate* truth = new SSAPredicate();
        truth->kind = SSAPredicate::True;
        return truth;
    }

    SSAPredicate* edgeCondition(BasicBlock* b1, BasicBlock* b2) {
        Instruction *term = b1->getTerminator();

        if (llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(term)) {
            for (unsigned i = 0; i < br->getNumSuccessors(); ++i) {
                if (br->getSuccessor(i) == b2 && br->isConditional()) {
                    SSAPredicate* condition = new SSAPredicate();
                    condition->kind = SSAPredicate::Kind::Condition;
                    condition->condition = br->getCondition();
                    if (i > 0) {  
                        SSAPredicate* no = new SSAPredicate();
                        no->kind = SSAPredicate::Kind::Not;
                        no->left = condition;
                        return no;
                    } else { 
                        return condition;
                    }
                }
            }
        }
        return truth();
    }

    SSAPredicate* getEdgePredicate(BasicBlock* from, BasicBlock* to)
    {
        Loop* loop = LI.getLoopFor(from);
        if (!loop) {
            return edgeCondition(from, to);
        }
        if (DT.dominates(to, from)) {
            return truth();
        }
        if (loop->getHeader() == from && LI.getLoopFor(to) == loop) {
            return truth();
        }
        Loop* toLoop = LI.getLoopFor(to);
        llvm::SmallVector<BasicBlock*> exitBlocks;
        loop->getExitBlocks(exitBlocks);
        for(BasicBlock* exit : exitBlocks) {
            if (exit == to) {
                errs() << "Exit edge from loop found: " << from->front() << " -> " << to->front() << "\n";
                SSAPredicate* first = new SSAPredicate();
                SSAPredicate* second = new SSAPredicate();
                first->kind = SSAPredicate::Kind::And;
                second->kind = SSAPredicate::Kind::And;
                second->left = simplifyPredicate(getControlPredicate(from));
                second->right = simplifyPredicate(edgeCondition(from, to));
                first->left = simplifyPredicate(getControlPredicate(loop->getLoopPreheader()));
                first->right = simplifyPredicate(second);
                return first;
            }
        }

        if (to->getSinglePredecessor() == from) {
            SSAPredicate* edgePred = edgeCondition(from, to);
            if (edgePred->kind == SSAPredicate::True) {
                return getControlPredicate(from);
            } else {
                SSAPredicate* first = new SSAPredicate();
                first->kind = SSAPredicate::Kind::And;
                first->left = simplifyPredicate(getControlPredicate(from));
                first->right = simplifyPredicate(edgePred);
                return first;
            }
        }
        
        
        SSAPredicate* first = new SSAPredicate();
        first->kind = SSAPredicate::Kind::And;
        first->left = simplifyPredicate(getControlPredicate(from));
        first->right = simplifyPredicate(edgeCondition(from, to));
        return first;
    }

    SSAPredicate *getControlPredicate(llvm::BasicBlock *BB)
    {
        auto it = predicateCache.find(BB);
        if (it != predicateCache.end())
        {
            return it->second;
        }
        std::vector<SSAPredicate *> preds;
        for (auto pred : predecessors(BB)) {
            SSAPredicate *edgePred = edgeCondition(pred, BB);
            if (edgePred->kind != SSAPredicate::True) {
                preds.push_back(edgePred);
            } else {
                SSAPredicate *predPred = getControlPredicate(pred);
                if (predPred->kind != SSAPredicate::True) {
                    preds.push_back(predPred);
                } else {
                    SSAPredicate *truePred = new SSAPredicate();
                    truePred->kind = SSAPredicate::True;
                    predicateCache[BB] = truePred;
                    return truePred;
                }
            }
        }
        if (preds.empty()) {
            SSAPredicate *truePred = new SSAPredicate();
            truePred->kind = SSAPredicate::True;
            predicateCache[BB] = truePred;
            return truePred;
        }
        SSAPredicate *result = preds[0];
        for (size_t i = 1; i < preds.size(); ++i) {
            SSAPredicate *orPred = new SSAPredicate();
            orPred->kind = SSAPredicate::Or;
            orPred->left = result;
            orPred->right = preds[i];
            result = simplifyPredicate(orPred);
        }
        predicateCache[BB] = result;
        return result;
    }

    std::unordered_map<Value *, SSAValue> valueMap;
    std::vector<Item> processBasicBlock(BasicBlock *BB, SSAPredicate *pred)
    {
        std::vector<Item> items;
        for (auto &I : *BB)
        {
            if (I.isTerminator() && !dyn_cast<ReturnInst>(&I)) continue;
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

        predicateCache[header] = getControlPredicate(preheader);
        llvm::SmallVector<std::pair<BasicBlock*, BasicBlock*>> exitBlocks;
        L->getExitEdges(exitBlocks);
        for(std::pair<BasicBlock*, BasicBlock*> exit : exitBlocks) {
            SSAPredicate* first = getEdgePredicate(exit.first, exit.second);
            if (ssaLoop->whileCondition == nullptr) {
                SSAPredicate* second = new SSAPredicate();
                second->kind = SSAPredicate::Kind::Not;
                second->left = first;
                ssaLoop->whileCondition = simplifyPredicate(second);
            } else {
                SSAPredicate* orPred = new SSAPredicate();
                orPred->kind = SSAPredicate::Kind::And;
                SSAPredicate* second = new SSAPredicate();
                second->kind = SSAPredicate::Kind::Not;
                second->left = first;
                orPred->left = ssaLoop->whileCondition;
                orPred->right = second;
                ssaLoop->whileCondition = simplifyPredicate(orPred);
            }
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
        std::vector<BasicBlock *> loopBlocks;
        for (auto *BB : L->blocks())
        {
            if(L->getHeader() == BB) continue;
            loopBlocks.push_back(BB);
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
                        loopItem.Predicate = getControlPredicate(L->getLoopPreheader());
                        ssaFunc->items.push_back(loopItem);
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
                // CLEAR BELOW
                auto items = processBasicBlock(BB, blockPred);
                ssaFunc->items.insert(ssaFunc->items.end(), items.begin(), items.end());
            }
        }

        return ssaFunc;
    }

    void clearPhiNode(PHINode *phi, BlockBuilder &bb)
    {
        // Avoid implementing by avoiding phi nodes altogether
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
        // Avoid implementing by avoiding phi nodes altogether
    }

    PHINode* eliminateMu(SSALoop::MuBinding *muNode, BasicBlock* header, BasicBlock* entry, BasicBlock* latch)
    {
        PHINode *node = PHINode::Create(muNode->muNode->type, 2, muNode->variable, header);

        if(auto init = std::get_if<llvm::Value*>(&muNode->muNode->init))
        {
            if(auto initMu = std::get_if<SSAMuNode*>(&muNode->muNode->init))
            {
                PHINode* initPhi = eliminateMu(new SSALoop::MuBinding{muNode->variable + "_init", *initMu}, header, entry, latch);
                node->addIncoming(initPhi, entry);
            }
        }
        if(auto rec = std::get_if<llvm::Value*>(&muNode->muNode->rec))
        {
            if(auto recMu = std::get_if<SSAMuNode*>(&muNode->muNode->rec))
            {
                PHINode* recPhi = eliminateMu(new SSALoop::MuBinding{muNode->variable + "_rec", *recMu}, header, entry, latch);
                node->addIncoming(recPhi, latch);
            }
        }
        return node;
    }

    BasicBlock* lowerToIR(std::variant<SSAFunction *, SSALoop *> function_or_loop, 
               BasicBlock *entry, LLVMContext &ctx)
    {
    BlockBuilder blockBuilder = BlockBuilder(entry, &VMap);
    eliminatePhiNodes(function_or_loop, blockBuilder);

    std::map<SSALoop::MuBinding*, PHINode*> bindingToPhi;

    if (auto loop = std::get_if<SSALoop *>(&function_or_loop))
    {
        errs() << "Lowering loop to IR, wish us luck\n";
        BasicBlock *header = BasicBlock::Create(ctx, "loop_header", entry->getParent());
        blockBuilder.append(header, (*loop)->whileCondition);
        BasicBlock *latch = BasicBlock::Create(ctx, "loop_latch", entry->getParent()); 
        BasicBlock *exit = BasicBlock::Create(ctx, "loop_exit", entry->getParent()); 
        for (auto& binding : (*loop)->muBindings) {
            PHINode* phi = eliminateMu(&binding, header, entry, latch);
            bindingToPhi[&binding] = phi;
            for (auto& pair : valueMap) {
                if (auto mu = std::get_if<SSAMuNode*>(&pair.second)) {
                    if (*mu == binding.muNode) {
                        VMap[pair.first] = phi;
                        break;
                    }
                }
            }
        } 
        BasicBlock *bodyBlock = header;
        for (size_t i = 0; i < (*loop)->bodyItems.size(); ++i)
        {
            auto &item = (*loop)->bodyItems[i];
        
            if (auto innerLoop = std::get_if<SSALoop *>(&item.content))
            {
                bodyBlock = lowerToIR(*innerLoop, bodyBlock, ctx);
                blockBuilder.append(bodyBlock, item.Predicate);
            }
            else if (auto instr = std::get_if<Instruction *>(&item.content))
            {
                Instruction *clone = (*instr)->clone();
                VMap[*instr] = clone;
                RemapInstruction(clone, VMap, RF_NoModuleLevelChanges);
                //errs() << "Inserting instruction: " << *clone << "\n";
                bodyBlock->getInstList().push_back(clone);
            }
        }
        blockBuilder.append(latch, truth());
        for (auto& binding : (*loop)->muBindings) {
            PHINode* phi = bindingToPhi[&binding];
            if(auto init = std::get_if<llvm::Value*>(&binding.muNode->init))
            {
                if(!std::get_if<SSAMuNode*>(&binding.muNode->init)) {
                    auto it = VMap.find(*init);
                    Value* remapped_init = (it != VMap.end()) ? (Value*)it->second : *init;
                    phi->addIncoming(remapped_init, entry);
                }
            }
            if(auto rec = std::get_if<llvm::Value*>(&binding.muNode->rec))
            {
                if(!std::get_if<SSAMuNode*>(&binding.muNode->rec)) {
                    auto it = VMap.find(*rec);
                    Value* remapped_rec = (it != VMap.end()) ? (Value*)it->second : *rec;
                    phi->addIncoming(remapped_rec, latch);
                }
            }
        }
        BasicBlock* block = blockBuilder.get_block((*loop)->whileCondition);
        BranchInst::Create(header, block);
        BranchInst::Create(exit, blockBuilder.getTrueBlock());
        return exit;
    }
    else if (auto function = std::get_if<SSAFunction *>(&function_or_loop))
    {
        BasicBlock *currentBlock = entry;
        
        for (size_t i = 0; i < (*function)->items.size(); ++i)
        {
            auto &item = (*function)->items[i];
            
            if (auto loop = std::get_if<SSALoop *>(&item.content))
            {
                currentBlock = lowerToIR(*loop, currentBlock, ctx);
            }
            else if (auto instr = std::get_if<Instruction *>(&item.content))
            {
                Instruction *clone = (*instr)->clone();
                VMap[*instr] = clone;
                RemapInstruction(clone, VMap, RF_NoModuleLevelChanges);
                //errs() << "Inserting instruction: " << *clone << "\n";
                currentBlock->getInstList().push_back(clone);
            }
        }
        
        blockBuilder.seal_off(currentBlock);
        // In theory we shouldn't need a terminator, as the last instruction should be one. If its not, complain!
        if (currentBlock->getTerminator() == nullptr) errs() << "Warning: Block " << currentBlock->front() << " has no terminator!\n";
    }
    restore_ssa(function_or_loop, blockBuilder);
    return entry;
}
};

SSAFunction *convertToPredicatedSSA(llvm::Function &llvmFunc)
{
    SSAPredicatedSSAConverter converter(llvmFunc);
    return converter.convertToPredicatedSSA();
}

void lowerToIR(SSAFunction *function, llvm::Function &llvmFunc)
{
    SSAPredicatedSSAConverter converter(llvmFunc);
    BasicBlock* newEntry = BasicBlock::Create(llvmFunc.getContext(), "entry", &llvmFunc);
    std::vector<BasicBlock *> OldBlocks;
    for (auto &BB : llvmFunc)
        if (&BB != newEntry)
            OldBlocks.push_back(&BB);
    converter.lowerToIR(function, newEntry, llvmFunc.getContext());
    newEntry->moveBefore(&llvmFunc.getEntryBlock());
    //verifyFunction(llvmFunc, &errs());
    //errs() << llvmFunc << "\n";
    //errs() << "We done lowering to IR.\n";
    for (auto *BB : OldBlocks)
        BB->dropAllReferences();
    for (auto *BB : OldBlocks)
        BB->eraseFromParent();
    //errs() << llvmFunc << "\n";
    std::vector<BasicBlock*> blocksToErase;
    for (auto &BB : llvmFunc)
        if (predecessors(&BB).empty() && &BB != &llvmFunc.getEntryBlock())
            blocksToErase.push_back(&BB);
    for (auto *BB : blocksToErase)
        BB->eraseFromParent();
    verifyFunction(llvmFunc, &errs());
    //errs() << "We done frfr.\n";
}
