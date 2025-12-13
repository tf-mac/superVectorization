#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

using namespace llvm;

namespace {

struct LICMPass : public PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &) {
        BasicBlock *Preheader = L.getLoopPreheader();
        std::unordered_set<Instruction*> toMove;
        int s = -1;
        while(toMove.size() != s) {
            s = toMove.size();
            for (auto& BB : L.blocks()) {
                for(auto& I : *BB) {
                    if(I.mayHaveSideEffects()) continue;

                    bool invariant = true;
                    for (auto &Op : I.operands()) {
                        Value *V = Op.get();
                        if (auto *OpI = dyn_cast<Instruction>(V)) {
                            if (L.contains(OpI) && !toMove.count(OpI)) {
                                invariant = false;
                                break;
                            }
                        }
                    }
                    if(invariant) {
                        toMove.insert(&I);
                    }
                }
            }
        }
        
        Instruction *Term = Preheader->getTerminator();
        for (auto *I : toMove) {
            I->moveBefore(Term);
        }
        return PreservedAnalyses::all();
    };
};

struct SkeletonPass : public PassInfoMixin<SkeletonPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
            for (auto &B : F) {
                for (auto it = B.begin(); it != B.end(); ) {
                    Instruction &I = *it++;
                    if(auto* op = dyn_cast<BinaryOperator>(&I)) {
                        if(op->getOpcode() == Instruction::Sub) {
                            IRBuilder<> builder(op);

                            Value *lhs = op->getOperand(0);
                            Value *rhs = op->getOperand(1);

                            Value *negRhs = builder.CreateNeg(rhs, "negrhs");

                            Value *newAdd = builder.CreateAdd(lhs, negRhs, "addneg");
                            op->replaceAllUsesWith(newAdd);
                            op->eraseFromParent();
                        } else if (op->getOpcode() == Instruction::FDiv) {
                            IRBuilder<> builder(op);

                            Value *lhs = op->getOperand(0);
                            Value *rhs = op->getOperand(1);
                            Type *ty = rhs->getType();
                            Value *one = ConstantFP::get(ty, 1.0);
                            Value *recip = builder.CreateFDiv(one, rhs, "recip");
                            Value *newMul = builder.CreateFMul(lhs, recip, "mulrecip");
                            
                            op->replaceAllUsesWith(newMul);
                            op->eraseFromParent();
                        }
                    }
                }
            }
        }
        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Slower mult/divide pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(SkeletonPass());
                    MPM.addPass(createModuleToFunctionPassAdaptor(
            createFunctionToLoopPassAdaptor(LICMPass())));
                });
        }
    };
}
