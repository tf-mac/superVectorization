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

struct VectorPack {
    std::vector<Instruction*> instructions;
    SSAPredicate* predicate;
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

bool operator==(const VectorPack& a, const VectorPack& b);

class SLPPacker {
private:
    std::unordered_map<Instruction*, SSAPredicate*> instructionPredicates;

    bool isUniformPredicate(const std::vector<Instruction*>& insts);

public:
    static bool isVectorizable(unsigned opcode);

    static bool predicatesEqual(SSAPredicate* a, SSAPredicate* b);

    std::unordered_set<VectorPack, PackHash> packInstructions(SSAFunction& function, int laneWidth);
};