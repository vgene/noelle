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
#pragma once

#include "SystemHeaders.hpp"

using namespace llvm;

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
  struct Region {
    BasicBlock const * start;
    BasicBlock const * end;
  };
  struct Node {
    Region region;
    std::vector<Region> children;
  };
}

namespace llvm {

  struct TalkDown : public ModulePass {
    using Annotation  = std::map<std::string, int64_t>;
    using Annotations = std::map<Instruction *, Annotation>;
    public:
      static char ID;

      TalkDown () : ModulePass{ ID } {}

      bool doInitialization (Module &M) override;
      bool runOnModule (Module &M) override;
      void getAnalysisUsage(AnalysisUsage &AU) const override;

    private:
      // TODO(jordan): coalesce BasicBlocks into an SESE tree.
      Annotations annotations;
  };
}
