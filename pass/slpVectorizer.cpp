
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <variant>
#include <string>
#include <memory>
#include <cassert>
#include <algorithm>
#include "predicatedSSA.h"
#include "slpVectorizer.h"

using namespace llvm;

bool operator==(const VectorPack &a, const VectorPack &b)
{
    return a.instructions == b.instructions;
}

static void buildMaps(std::unordered_map<Instruction *, SSAPredicate *> &instructionPredicates, const std::vector<Item> &items)
{
    for (const auto &item : items)
    {
        if (std::holds_alternative<llvm::Instruction *>(item.content))
        {
            auto *inst = std::get<llvm::Instruction *>(item.content);
            instructionPredicates[inst] = item.Predicate;
        }
        else
        {
            auto *loop = std::get<SSALoop *>(item.content);
            buildMaps(instructionPredicates, loop->bodyItems);
        }
    }
}

static std::vector<std::vector<Instruction *>> findSeeds(const std::unordered_map<Instruction *, SSAPredicate *> &instructionPredicates, const std::vector<Item> &items)
{
    std::vector<std::vector<Instruction *>> seeds;
    std::vector<Instruction *> currentGroup;
    unsigned lastOpcode = 0;
    SSAPredicate *lastPred = nullptr;

    for (const auto &item : items)
    {
        if (std::holds_alternative<llvm::Instruction *>(item.content))
        {
            auto *inst = std::get<llvm::Instruction *>(item.content);
            unsigned opcode = inst->getOpcode();
            SSAPredicate *pred = instructionPredicates.at(inst);

            if (!SLPPacker::isVectorizable(opcode))
            {
                continue;
            }

            if (opcode == lastOpcode && SLPPacker::predicatesEqual(pred, lastPred))
            {
                currentGroup.push_back(inst);
            }
            else
            {
                if (currentGroup.size() >= 2)
                {
                    seeds.push_back(currentGroup);
                }
                currentGroup = {inst};
                lastOpcode = opcode;
                lastPred = pred;
            }
        }
        else
        {
            if (!currentGroup.empty())
            {
                if (currentGroup.size() >= 2)
                {
                    seeds.push_back(currentGroup);
                }
                currentGroup.clear();
                lastOpcode = 0;
                lastPred = nullptr;
            }
            auto *loop = std::get<SSALoop *>(item.content);
            auto loopSeeds = findSeeds(instructionPredicates, loop->bodyItems);
            seeds.insert(seeds.end(), loopSeeds.begin(), loopSeeds.end());
        }
    }

    if (currentGroup.size() >= 2)
    {
        seeds.push_back(currentGroup);
    }

    return seeds;
}

// For simplicity we consider only a few operations. Can easily be expanded
bool SLPPacker::isVectorizable(unsigned opcode)
{
    return opcode == Instruction::Add || opcode == Instruction::FAdd ||
           opcode == Instruction::Mul || opcode == Instruction::FMul ||
           opcode == Instruction::Load || opcode == Instruction::Store;
}

bool SLPPacker::predicatesEqual(SSAPredicate *a, SSAPredicate *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind)
    {
    case SSAPredicate::True:
        return true;
    case SSAPredicate::Condition:
        return a->condition == b->condition;
    case SSAPredicate::Not:
        return predicatesEqual(a->left, b->left);
    case SSAPredicate::And:
    case SSAPredicate::Or:
        return predicatesEqual(a->left, b->left) && predicatesEqual(a->right, b->right);
    default:
        return false;
    }
}

bool SLPPacker::isUniformPredicate(const std::vector<Instruction *> &insts)
{
    if (insts.empty())
        return true;

    SSAPredicate *firstPred = instructionPredicates.at(insts[0]);
    for (size_t i = 1; i < insts.size(); i++)
    {
        if (!predicatesEqual(instructionPredicates.at(insts[i]), firstPred))
        {
            return false;
        }
    }
    return true;
}

std::unordered_set<VectorPack, PackHash> SLPPacker::packInstructions(SSAFunction &function, int laneWidth)
{
    instructionPredicates.clear();
    buildMaps(instructionPredicates, function.items);
    auto seeds = findSeeds(instructionPredicates, function.items);

    std::unordered_set<VectorPack, PackHash> packs;

    for (const auto &seedGroup : seeds)
    {
        if (seedGroup.size() >= 2 && static_cast<int>(seedGroup.size()) <= laneWidth && isUniformPredicate(seedGroup))
        {
            VectorPack pack;
            pack.instructions = seedGroup;
            pack.predicate = instructionPredicates[seedGroup[0]];
            packs.insert(pack);
        }
    }

    std::unordered_map<Instruction *, int> instToIndex;
    for (int i = 0; i < function.items.size(); i++)
    {
        if (std::holds_alternative<llvm::Instruction *>(function.items[i].content))
        {
            instToIndex[std::get<llvm::Instruction *>(function.items[i].content)] = i;
        }
    }

    std::unordered_set<VectorPack, PackHash> goodPacks;
    for (const auto &pack : packs)
    {
        std::vector<int> indices;
        for (auto *inst : pack.instructions)
            indices.push_back(instToIndex[inst]);
        int min_index = *std::min_element(indices.begin(), indices.end());
        int max_index = *std::max_element(indices.begin(), indices.end());
        bool canVectorize = true;
        for (auto *inst : pack.instructions)
        {
            for (auto user : inst->users())
            {
                if (auto *userInst = llvm::dyn_cast<llvm::Instruction>(user))
                {
                    if (instToIndex.count(userInst) && instToIndex[userInst] < min_index)
                    {
                        canVectorize = false;
                        break;
                    }
                }
            }
            if (!canVectorize)
            {
                errs() << "Vectorization failed :(\n";
                break;
            }
        }
        if (canVectorize)
        {
            std::vector<Item> newItems;
            std::unordered_set<int> packIndices(indices.begin(), indices.end());
            instToIndex.clear();
            for (int i = 0; i < function.items.size(); i++)
            {
                if (packIndices.count(i))
                {
                    if (i == min_index)
                    {
                        for (size_t j = 0; j < indices.size(); j++)
                        {
                            newItems.push_back(function.items[indices[j]]);
                        }
                    }
                }
                else
                {
                    newItems.push_back(function.items[i]);
                }
                if (std::holds_alternative<llvm::Instruction *>(newItems[i].content))
                {
                    instToIndex[std::get<llvm::Instruction *>(newItems[i].content)] = i;
                }
            }
            function.items = newItems;
            goodPacks.insert(pack);
        }
    }
    return goodPacks;
}