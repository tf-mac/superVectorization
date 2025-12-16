
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <variant>
#include <string>
#include <memory>
#include <cassert>
#include "predicatedSSA.h"

using namespace llvm;

struct InstInfo {
    unsigned opcode;
    Type* type;
};

struct VectorPack {
    std::vector<Instruction*> instructions;
    unsigned opcode;
    SSAPredicate* predicate;
    Type* elementType;
};

struct PackHash {
    size_t operator()(const VectorPack& pack) const {
        size_t hash = 0;
        for (auto* inst : pack.instructions) {
            hash ^= std::hash<Instruction*>()(inst);
        }
        return hash;
    }
};

bool operator==(const VectorPack& a, const VectorPack& b) {
    return a.instructions == b.instructions;
}

class SLPPacker {
private:
    std::unordered_map<Instruction*, SSAPredicate*> instructionPredicates;
    std::unordered_map<Instruction*, InstInfo> instInfo;

    static bool isVectorizable(unsigned opcode) {
        return opcode == Instruction::Add || opcode == Instruction::FAdd ||
               opcode == Instruction::Mul || opcode == Instruction::FMul ||
               opcode == Instruction::Load || opcode == Instruction::Store;
    }

    static bool predicatesEqual(SSAPredicate* a, SSAPredicate* b) {
        if (a == b) return true;
        if (!a || !b) return false;
        if (a->kind != b->kind) return false;
        switch (a->kind) {
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

    bool isUniformPredicate(const std::vector<Instruction*>& insts) {
        if (insts.empty()) return true;
        
        SSAPredicate* firstPred = instructionPredicates.at(insts[0]);
        for (size_t i = 1; i < insts.size(); i++) {
            if (!predicatesEqual(instructionPredicates.at(insts[i]), firstPred)) {
                return false;
            }
        }
        return true;
    }

    void buildMaps(const std::vector<Item>& items) {
        for (const auto& item : items) {
            if (std::holds_alternative<llvm::Instruction*>(item.content)) {
                auto* inst = std::get<llvm::Instruction*>(item.content);
                instructionPredicates[inst] = item.Predicate;
                InstInfo info;
                info.opcode = inst->getOpcode();
                info.type = inst->getType();
                instInfo[inst] = info;
            } else {
                auto* loop = std::get<SSALoop*>(item.content);
                buildMaps(loop->bodyItems);
            }
        }
    }

    std::vector<std::vector<Instruction*>> findSeeds(const std::vector<Item>& items) {
        std::vector<std::vector<Instruction*>> seeds;
        std::vector<Instruction*> currentGroup;
        unsigned lastOpcode = 0;
        SSAPredicate* lastPred = nullptr;

        for (const auto& item : items) {
            if (std::holds_alternative<llvm::Instruction*>(item.content)) {
                auto* inst = std::get<llvm::Instruction*>(item.content);
                unsigned opcode = inst->getOpcode();
                SSAPredicate* pred = instructionPredicates[inst];

                if (!isVectorizable(opcode)) {
                    if (!currentGroup.empty()) {
                        if (currentGroup.size() >= 2) {
                            seeds.push_back(currentGroup);
                        }
                        currentGroup.clear();
                        lastOpcode = 0;
                        lastPred = nullptr;
                    }
                    continue;
                }

                if (opcode == lastOpcode && predicatesEqual(pred, lastPred)) {
                    currentGroup.push_back(inst);
                } else {
                    if (currentGroup.size() >= 2) {
                        seeds.push_back(currentGroup);
                    }
                    currentGroup = {inst};
                    lastOpcode = opcode;
                    lastPred = pred;
                }
            } else {
                if (!currentGroup.empty()) {
                    if (currentGroup.size() >= 2) {
                        seeds.push_back(currentGroup);
                    }
                    currentGroup.clear();
                    lastOpcode = 0;
                    lastPred = nullptr;
                }
                auto* loop = std::get<SSALoop*>(item.content);
                auto loopSeeds = findSeeds(loop->bodyItems);
                seeds.insert(seeds.end(), loopSeeds.begin(), loopSeeds.end());
            }
        }

        if (currentGroup.size() >= 2) {
            seeds.push_back(currentGroup);
        }

        return seeds;
    }

public:
    std::unordered_set<VectorPack, PackHash> packInstructions(SSAFunction& function, int laneWidth) {
        buildMaps(function.items);
        auto seeds = findSeeds(function.items);
        
        std::unordered_set<VectorPack, PackHash> packs;
        
        for (const auto& seedGroup : seeds) {
            if (seedGroup.size() >= 2 && isUniformPredicate(seedGroup)) {
                VectorPack pack;
                pack.instructions = seedGroup;
                pack.opcode = instInfo[seedGroup[0]].opcode;
                pack.elementType = instInfo[seedGroup[0]].type;
                pack.predicate = instructionPredicates[seedGroup[0]];
                
                packs.insert(pack);
            }
        }
        
        return packs;
    }
};