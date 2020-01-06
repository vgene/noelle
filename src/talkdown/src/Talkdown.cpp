#include "SystemHeaders.hpp"

#include "Talkdown.hpp"
#include "Node.hpp"

#include <iostream>
#include <map>
#include <string>

using namespace llvm;

namespace llvm
{

  bool Talkdown::runOnModule(Module &M)
  {
    // std::cerr << "Before initialization\n";
    // doInitialization( M );
    // std::cerr << "After initialization\n";
    std::cerr << "Should be initialized\n";

    std::cerr << "There are " << function_trees.size() << " function trees\n";

    for ( auto &tree : function_trees )
    {
      std::cerr << tree;
    }

    return false;
  }

  void Talkdown::getAnalysisUsage(AnalysisUsage &AU)
  {
    AU.setPreservesAll();
  }

  bool Talkdown::doInitialization(Module &M)
  {
    bool modified = false;
    std::cerr << "Functions in module:\n";
    for ( auto &f : M )
    {
      std::cerr << "\t" << f.getName().str() << "\n";
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
