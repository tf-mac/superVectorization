
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


class SLPPacker {
private:
    bool isUniformPredicate(const std::vector<Instruction*>& insts, 
                           const std::unordered_map<Instruction*, SSAPredicate*>& predMap) {
        if (insts.empty()) return true;
        
        SSAPredicate* firstPred = predMap.at(insts[0]);
        for (size_t i = 1; i < insts.size(); i++) {
            if (!predicatesEqual(predMap.at(insts[i]), firstPred)) {
                return false;
            }
        }
        return true;
    }

    
    
public:
    std::unordered_set<std::vector<Instruction*>> packInstructions(SSAFunction& function, int laneWidth) {
        auto seeds = findSeeds(function.items, instructionPredicates);
        
        std::unordered_set<VectorPack, PackHash> packs;
        std::unordered_set<Instruction*> visited;
        
        for (const auto& seedGroup : seeds) {
            for (Instruction* seed : seedGroup) {
                if (!visited.count(seed)) {
                    std::vector<Instruction*> currentPack;
                    
                    if (currentPack.size() >= 2) {
                        VectorPack pack;
                        pack.instructions = currentPack;
                        // pack.elementType = instInfo[currentPack[0]].type;
                        pack.opcode = instInfo[currentPack[0]].opcode;
                        pack.predicate = instructionPredicates[currentPack[0]];
                        
                        packs.insert(pack);
                    }
                }
            }
        }
        
        if (hasCircularDependency(packs)) {
            packs.clear();
        }
        
        return packs;
    }
};