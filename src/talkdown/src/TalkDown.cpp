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
using Annotation = std::map<std::string, int64_t>;

Annotation parse_metadata (MDNode * md) {
  // NOTE(jordan): MDNode is a tuple of MDString, ConstantInt pairs
  // NOTE(jordan): Use mdconst::dyn_extract API from Metadata.h#483
  Annotation result = {};
  for (auto const & pair_operand : md->operands()) {
    using namespace llvm;
    auto * pair = dyn_cast<MDNode>(pair_operand.get());
    auto * key = dyn_cast<MDString>(pair->getOperand(0));
    auto * val = mdconst::dyn_extract<ConstantInt>(pair->getOperand(1));
    result.emplace(key->getString(), val->getSExtValue());
  }
  return result;
}

void print_annotation (Annotation value, llvm::raw_ostream & os) {
  os << "Annotation {\n";
  for (auto annotation_entry : value) {
    os
      << "  "  << annotation_entry.first
      << " = " << annotation_entry.second
      << "\n";
  }
  os << "};\n";
}

namespace SESE {
  /* NOTE(jordan): requirements:
   *
   * 1. Given an instruction, obtain the correct Noelle metadata
   * 2. Figure out the range of an annotation (?) (coalescing)
   *
   * The original idea was - what? To be able to, for a particular
   * instruction, figure out the applicable annotation(s) -- this is
   * already done. We flatten the nested annotation structure earlier in
   * the process. So we can just look up the annotation, at this point. We
   * need to deserialize it from the instruction and store it for later.
   *
   * TODO(jordan): discuss with Simone the API we still need, if the SESE
   * tree isn't used anymore to apply scoping rules... Do we need more
   * than to be able to say, for a particulary Basic Block, "this is the
   * applicable Note-Noelle annotation"? (e.g. ordered, independent, etc.)
   *
   */
  std::map<Instruction *, Annotation> InstructionAnnotations;
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
          print_annotation(parse_metadata(last_note_meta), llvm::errs());
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

    BasicBlock::iterator I (split);
    Instruction * previous = &*--I;
    if (previous->hasMetadata()) {
      MDNode * noelle_meta = previous->getMetadata("note.noelle");
      llvm::errs() << previous << " has Noelle annotation:\n";
      print_annotation(parse_metadata(noelle_meta), llvm::errs());
    }

    // NOTE(jordan): using SplitBlock is recommended in the docs
    llvm::SplitBlock(split->getParent(), split);
  }

  llvm::errs() << "Splits made.\n";
  // Construct SESE tree
  // TODO

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
