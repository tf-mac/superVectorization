#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "predicatedSSA.h"
#include "slpVectorizer.h"

using namespace llvm;

namespace {

struct SuperVectorizationPass : public PassInfoMixin<SuperVectorizationPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
            SSAFunction* PredF = convertToPredicatedSSA(F);
            //PredicatedSSAPrinter::print(PredF, errs());
            SLPPacker packer;
            auto packs = packer.packInstructions(*PredF, 4);
            errs() << "Found " << packs.size() << " vector packs\n";
            lowerToIR(PredF, F);
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
                        MPM.addPass(createModuleToFunctionPassAdaptor(
                            LoopSimplifyPass()));
                        MPM.addPass(SuperVectorizationPass());
                        MPM.addPass(createModuleToFunctionPassAdaptor(
                            LoopSimplifyPass()));
                        FunctionPassManager FPM;

                        
                        return true;   
                    }
                    return false;
                });
            
            PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
        }
    };
}