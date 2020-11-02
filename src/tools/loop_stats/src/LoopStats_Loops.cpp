/*
 * Copyright 2019 - 2020  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "LoopStats.hpp"

using namespace llvm;
using namespace llvm::noelle;

void LoopStats::collectStatsForLoop (Hot *profiles, int id, ScalarEvolution &SE, PDG *loopDG, Loop &llvmLoop) {
  auto statsForLoop = new Stats();
  statsByLoopAccordingToLLVM.insert(std::make_pair(id, statsForLoop));
  statsForLoop->loopID = id;

  auto loopFunction = llvmLoop.getHeader()->getParent();

  collectStatsOnLLVMIVs(profiles, SE, llvmLoop, statsForLoop);
  collectStatsOnLLVMInvariants(profiles, llvmLoop, statsForLoop);
  collectStatsOnLLVMSCCs(profiles, loopDG, statsForLoop);

  /*
   * Compute the coverage for the loop.
   */
  for (auto bb : llvmLoop.getBlocks()){
    statsForLoop->dynamicTotalInstructions = profiles->getTotalInstructions(bb);
  }

  return ;
}

void LoopStats::collectStatsForLoop (Hot *profiles, LoopDependenceInfo &LDI) {
  auto loopStructure = LDI.getLoopStructure();
  auto statsForLoop = new Stats();
  statsByLoopAccordingToNoelle.insert(std::make_pair(LDI.getID(), statsForLoop));
  statsForLoop->loopID = loopStructure->getID();
  errs() << "First instruction in loop " << statsForLoop->loopID << ": " << *loopStructure->getEntryInstruction() << "\n";

  collectStatsOnNoelleIVs(profiles, LDI, statsForLoop);
  collectStatsOnNoelleSCCs(profiles, LDI, statsForLoop);
  collectStatsOnNoelleInvariants(profiles, LDI, statsForLoop);

  /*
   * Compute the coverage for the loop.
   */
  statsForLoop->dynamicTotalInstructions = profiles->getTotalInstructions(loopStructure);
  auto loopDG = LDI.getLoopDG();
  for ( auto edge : make_range(loopDG->begin_edges(), loopDG->end_edges()) )
  {
    // don't count edges that come from outside
    if ( !loopDG->isInternal(edge->getIncomingT()) ||
         !loopDG->isInternal(edge->getOutgoingT()) ||
         !edge->isLoopCarriedDependence() )
      continue;
    errs() << "LC edge:\n";
    edge->print(errs()) << "\n";
    /* edge->print() << "\n"; */
    statsForLoop->totalLCEdges++;
  }


  return ;
}
