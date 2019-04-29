/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:

 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "TalkDown.hpp"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CFG.h"

bool llvm::TalkDown::doInitialization (Module &M) {
  return false;
}

/* FIXME(jordan): this is copied from the types/utilities in pragma-note.
 *
 * - Annotation (type)
 * - parse_annotation (MDNode *   -> Annotation)
 * - print_annotation (Annotation -> void)
 *
 * These even live in different files. It is not obvious how one would go
 * about modularizing them cleanly in the pragma-note codebase, or (with
 * the exception perhaps of using git submodules) how that codebase could
 * reasonably be copied into this one for easy reference.
 */
namespace Note {
  TalkDown::Annotation parse_metadata (MDNode * md) {
    // NOTE(jordan): MDNode is a tuple of MDString, ConstantInt pairs
    // NOTE(jordan): Use mdconst::dyn_extract API from Metadata.h#483
    TalkDown::Annotation result = {};
    for (auto const & pair_operand : md->operands()) {
      using namespace llvm;
      auto * pair = dyn_cast<MDNode>(pair_operand.get());
      auto * key = dyn_cast<MDString>(pair->getOperand(0));
      auto * val = mdconst::dyn_extract<ConstantInt>(pair->getOperand(1));
      result.emplace(key->getString(), val->getSExtValue());
    }
    return result;
  }

  void print_annotation (TalkDown::Annotation value, llvm::raw_ostream & os) {
    os << "Annotation {\n";
    for (auto annotation_entry : value) {
      os
        << "  "  << annotation_entry.first
        << " = " << annotation_entry.second
        << "\n";
    }
    os << "};\n";
  }
}

bool llvm::TalkDown::runOnModule (Module &M) {
  /* 1. Split BasicBlocks wherever the applicable annotation changes
   * 2. Construct SESE tree at BasicBlock granularity; write query APIs
   */

  using SplitPoint = Instruction *;
  llvm::SmallVector<SplitPoint, 8> splits;

  // Collect all the split points in each function
  // TODO(jordan): it looks like this can be refactored as a FunctionPass
  for (auto & function : M) {
    MDNode * last_note_meta = nullptr;
    for (auto & block : function) {
      for (auto & instruction : block) {
        /* NOTE(jordan): When there's a new annotation (or none, when
         * there was one; or one, where there was none), we need to split
         * the block.
         */
        if (true
          && instruction.hasMetadata()
          && (instruction.getMetadata("note.noelle") != last_note_meta)
        ) {
          splits.emplace_back(&instruction);
          last_note_meta = instruction.getMetadata("note.noelle");
          llvm::errs() << instruction << " has Noelle annotation:\n";
          Annotation note = Note::parse_metadata(last_note_meta);
          Note::print_annotation(note, llvm::errs());
          this->annotations.insert({ &instruction, note });
        }
      }
    }
  }

  llvm::errs() << "Split points constructed: " << splits.size() << "\n";

  // Perform splitting
  for (SplitPoint & split : splits) {
    llvm::errs()
      << "Split:"
      << "\n\tblock @ " << split->getParent()
      << "\n\tinstruction @ " << split
      << "\n";

    // NOTE(jordan): DEBUG
    /* BasicBlock::iterator I (split); */
    /* Instruction * previous = &*--I; */
    /* if (previous->hasMetadata()) { */
    /*   MDNode * noelle_meta = previous->getMetadata("note.noelle"); */
    /*   llvm::errs() << previous << " has Noelle annotation:\n"; */
    /*   Annotation note = Note::parse_metadata(noelle_meta); */
    /*   Note::print_annotation(note, llvm::errs()); */
    /* } */

    // NOTE(jordan): using SplitBlock is recommended in the docs
    llvm::SplitBlock(split->getParent(), split);
  }

  llvm::errs() << "Splits made.\n";
  // Construct SESE tree
  for (auto const & function : M) {
    std::vector<BasicBlock const *> region_starts;
    std::vector<SESE::Region> regions;
    llvm::errs() << "Find regions.\n";
    for (auto const & block : function) {
      int predecessor_count = 0, successor_count = 0;
      auto predecessors = llvm::predecessors(&block);
      auto successors   = llvm::successors(&block);
      for (auto pred : predecessors) { predecessor_count++; }
      for (auto succ : predecessors) { successor_count++; }
      if (predecessor_count == 1 && successor_count > 1) {
        // NOTE(jordan): This delineates the start of a region
        llvm::errs() << "Found region start.\n";
        region_starts.push_back(&block);
      } else if (predecessor_count == 1 && successor_count == 1) {
        // NOTE(jordan): This block is internal to a region
        llvm::errs() << "Found inner region.\n";
        regions.push_back({ &block, &block });
      } else if (predecessor_count > 1 && successor_count == 1) {
        // NOTE(jordan): This delineates the end of a region
        // FIXME(jordan): fuck this.
        llvm::errs() << "Found region end.\n";
        BasicBlock * start = const_cast<BasicBlock *>(&block);
        while (std::find(region_starts.begin(), region_starts.end(), start) == region_starts.end()) {
          start = const_cast<BasicBlock *>(&**llvm::pred_begin(start));
        }
        llvm::errs() << "start for region: " << *start << "\n";
        llvm::errs() << "end   for region: " << block  << "\n";
      }
    }
  }

  return true; // blocks are split; source is modified
}

void llvm::TalkDown::getAnalysisUsage (AnalysisUsage &AU) const {
  /* NOTE(jordan): I'm pretty sure this analysis is non-preserving of
   * other analyses. Control flow changes, for example, when basic blocks
   * are split. It would be difficult to not do this, but possible.
   */
  /* AU.setPreservesAll(); */
  return ;
}

// Register pass with LLVM
char llvm::TalkDown::ID = 0;
static RegisterPass<TalkDown> X("TalkDown", "The TalkDown pass");

// Register pass with Clang
static TalkDown * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new TalkDown());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new TalkDown());}});// ** for -O0
