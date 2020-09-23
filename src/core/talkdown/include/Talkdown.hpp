#pragma once

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "Node.hpp"
#include "Tree.hpp"
#include "Annotation.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <unordered_set>

using namespace llvm;
using namespace AutoMP;

namespace llvm
{
  struct Talkdown : public ModulePass
  {
    public:
      static char ID;

      Talkdown() : ModulePass( ID ) {}

      bool doInitialization(Module &M);
      bool runOnModule(Module &M);
      void getAnalysisUsage(AnalysisUsage &AU) const;

      bool containsAnnotation(Loop *) const;
      const AnnotationSet &getAnnotationsForInst(Instruction *i) const;
      const AnnotationSet &getAnnotationsForInst(Instruction *i, Loop *l) const;

    private:
      bool enabled;

      std::vector<FunctionTree> function_trees;

      const FunctionTree &findTreeForFunction(Function *f) const;
  };
} // namespace llvm
