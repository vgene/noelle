#include "TempDebugInfo.hpp"

using namespace llvm;

unsigned int getLoopLineNum(const Loop *l)
{
  if ( !l )
    return 0;

  MDNode *loopMD = l->getLoopID();

  if ( loopMD )
  {
    auto *diloc = dyn_cast<DILocation>(loopMD->getOperand(1));
    auto dbgloc = DebugLoc(diloc);
    return dbgloc.getLine();
  }

  return 0;
}
