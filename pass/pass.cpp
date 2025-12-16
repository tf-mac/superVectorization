#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "predicatedSSA.h"

using namespace llvm;

namespace {

struct SuperVectorizationPass : public PassInfoMixin<SuperVectorizationPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
            SSAFunction* PredF = convertToPredicatedSSA(F);
            // for (auto I : PredF->items) {
            //     if(auto instr = std::get_if<llvm::Instruction*>(&I.content)) {
            //         errs() << (**instr) << "\n";
            //     } else if(auto loop = std::get_if<SSALoop*>(&I.content)){
            //         errs() << "LOOP " << (*loop)->bodyItems.size() << "\n";
            //     }
            // }
            PredicatedSSAPrinter::print(PredF, errs());
            //lowerToIR(PredF, F);
        }

        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "SuperVectorization pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "super-vectorization") {
                        MPM.addPass(SuperVectorizationPass());
                        return true;   
                    }
                    return false;
                });
            
            
        }
    };
}