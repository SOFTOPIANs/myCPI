#ifndef LLVM_CORELAB_STAND_ALONE_PARTITIONER_H
#define LLVM_CORELAB_STAND_ALONE_PARTITIONER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class StandAlonePartitioner : public ModulePass {
    public:
      static char ID;

      StandAlonePartitioner() : ModulePass (ID) {}
      
      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "STAND_ALONE_PARTITIONER"; }

      bool runOnModule(Module &M);

      void setFunctions(Module &M);
    private:
      Constant *VoidFunc;
  };
}

#endif
