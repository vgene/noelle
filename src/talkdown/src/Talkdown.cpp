#include "SystemHeaders.hpp"

#include "Talkdown.hpp"
#include "Node.hpp"

#include <map>
#include <string>

using namespace llvm;

namespace llvm
{

  bool Talkdown::runOnModule(Module &M)
  {
    return false;
  }

  void Talkdown::getAnalysisUsage(AnalysisUsage &AU)
  {
    AU.setPreservesAll();
  }

  bool Talkdown::doInitialization(Module &M)
  {
    bool modified = false;
    for ( auto &f : M )
    {
      FunctionTree tree = FunctionTree( &f );
      modified |= tree.constructTree( &f );
      function_trees.push_back( tree );
    }

    return modified;
  }

  SESENode *Talkdown::getInnermostRegion(Instruction *inst)
  {
    return nullptr;
  }

  SESENode *Talkdown::getParent(SESENode *node)
  {
    return nullptr;
  }

  SESENode *Talkdown::getInnermostCommonAncestor(SESENode *node1, SESENode *node2)
  {
    return nullptr;
  }

  std::map<std::string, std::string> &Talkdown::getMetadata(SESENode *node)
  {
    std::map<std::string, std::string> meta = {};

    return meta;
  }

} // namespace llvm

char llvm::Talkdown::ID = 0;
static RegisterPass<Talkdown> X("Talkdown", "The Talkdown pass");

// Register pass with Clang
static Talkdown * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new Talkdown());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new Talkdown());}});// ** for -O0
