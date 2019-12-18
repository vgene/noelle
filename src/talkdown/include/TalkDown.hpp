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

namespace Note {
  using Annotation = std::map<std::string, std::string>;
  Annotation parse_metadata (MDNode * md) {
    // NOTE(jordan): MDNode is a tuple of MDString, MDString pairs
    // NOTE(jordan): Use mdconst::dyn_extract API from Metadata.h#483
    Annotation result = {};
    for (auto const & pair_operand : md->operands()) {
      using namespace llvm;
      pair_operand->dump();
      auto * pair = dyn_cast<MDNode>(pair_operand.get());
      auto * key = dyn_cast<MDString>(pair->getOperand(0));
      auto * val = dyn_cast<MDString>(pair->getOperand(1));
      result.emplace(key->getString(), val->getString());
    }
    return result;
  }

  void print_annotation (Annotation value, llvm::raw_ostream & os) {
    os << "Annotation {\n";
    for (auto annotation_entry : value) {
      os
        << "\t"  << annotation_entry.first
        << " = " << annotation_entry.second
        << "\n";
    }
    os << "};";
  }
}

// probably remove this namespace at some point
namespace SESE {

template <typename NodeTy>
  struct AbstractEdge : std::pair<NodeTy const *, NodeTy const *> {
    AbstractEdge (NodeTy const * a, NodeTy const * b)
      : std::pair<NodeTy const *, NodeTy const *> (a, b)
      {};
    bool touches (NodeTy const * n) const {
      return n == this->first || n == this->second;
    }
    std::pair<bool, NodeTy const *> other_end (NodeTy const * node) const {
      if (node == this->first) {
        return { true, this->second };
      } else if (node == this->second) {
        return { true, this->first };
      } else {
        return { false, nullptr };
      }
    }
  };

namespace UndirectedCFG {
  struct Node {
    uint64_t number;
    BasicBlock * block;
  };
  using Edge = SESE::AbstractEdge<Node>;
  struct Graph {
    bool valid;
    bool empty;
    Node const * exit;
    std::vector<Node> const nodes;
    std::vector<Edge> const edges;
  };
} // namespace UnidirectedCFG

namespace SpanningTree {
  struct Node {
    Node (int dfs_index, BasicBlock * block, Node const * parent)
      : dfs_index(dfs_index), block(block), parent(parent)
      {}
    int dfs_index; // index in tree (simplifies cycle equiv.)
    BasicBlock * block;
    Node const * parent;
    std::vector<Node const *> children;
    std::vector<Node const *> backedges;
    std::vector<UndirectedCFG::Edge const *> cfg_backedges;
  };
  using Backedge = SESE::AbstractEdge<Node>;
  struct Tree {
    bool valid;
    bool empty;
    Node const * root;
    Node const * exit;
    std::vector<Node *> nodes; // nodes ordered by visitation order
    std::vector<Backedge> backedges; // NOTE: back-edges are unordered
  };
} // namespace SpanningTree

namespace CycleEquivalence {
  struct BracketList; // NOTE(jordan): forward declarations for Edge.
  struct Node;
  using EdgeBase = SESE::AbstractEdge<Node>;
  struct Edge : EdgeBase {
    // The Program Structure Tree. Section 3.5. p177
    int cycle_class  = -1; // index of cycle equivalence class
    int recent_size  = -1; // size of bracket set when this was top edge
    int recent_class = -1; // equiv. class no. of *tree* edge where this
                           //   was most recently the topmost bracket
    Edge (Node const * a, Node const * b) : EdgeBase(a, b) {};
  };
  struct BracketList {
    // The Program Structure Tree. Section 3.5. p177
    std::vector<Edge *> brackets;
    // create() : BracketList
    BracketList () {}
    // size (bl: BracketList) : integer
    int size () { return brackets.size(); }
    // push (bl: BracketList, e: bracket): BracketList
    void push (Edge * edge) { brackets.push_back(edge); }
    // top (bl: BracketList) : bracket
    Edge * top () { return brackets.back(); }
    // delete (bl: BracketList, e: bracket) : BracketList
    void del (Edge const * edge) {
      auto last_removed = std::remove(
        std::begin(brackets),
        std::end(brackets),
        edge
      );
      brackets.erase(last_removed, brackets.end());
    }
    // concat (bl1, bl2: BracketList) : BracketList
    void concat (BracketList const & other) {
      for (auto & other_bracket : other.brackets) {
        push(other_bracket);
      }
    }
  };
  struct Node {
    // The Program Structure Tree. Section 3.5. p177
    int hi;        // dfs_index of dest. nearest to root from a descndnt
    int dfs_index; // depth-first search index of node ("dfsnum")
    BracketList bracket_list;
    std::vector<int> children;  // referenced by dfs-index
    std::vector<int> backedges; // referenced by dfs-index
    int parent;
    BasicBlock * block;
    bool descends_from (Node const &, std::vector<Node> const &) const;
  };
  struct Graph {
    bool valid;
    bool empty;
    int cycle_classes;
    Node const * start;
    Node const * exit;
    std::vector<Node> const nodes;
    std::vector<Edge> const edges;
    std::vector<Edge> const backedges;
    std::map<Node const *, std::vector<Edge *>> const tree_map;
    std::map<Node const *, std::vector<Edge *>> const backedge_map;
    /* NOTE(jordan): capping backedges are only applicable to unstructured
     * loops in complex control flow. Unstructured loops are theoretically
     * handled by the algorithm, but we do not support them / haven't
     * tested for them.
     */
    /* NOTE(jordan): if we make this const, we cannot `move` a constructed
     * capping_backedges map into a Graph at the end of
     * `Graph::compute()`, so... fuck's up with us, yeah? The C++ standard
     * is totally intuitive. Fuck you. Yes, of course moving a non-const
     * type into a const type causes an implicit copy when you use
     * std::move, and doesn't do something like, I don't know, fail and
     * tell you that your `move` is actually a copy? BEcaUse that would be
     * fucking reasonable, wouldn't it? No, it just copies a move. Because
     * that's what you wanted.
     */
    std::map<Node const *, std::vector<std::unique_ptr<Edge>>>
    capping_backedges;
    static Graph compute (SpanningTree::Tree const &);
    static void print (Graph const &, llvm::raw_ostream &, bool);
    static void print_brackets (Graph const &, llvm::raw_ostream &);
  };
} // namespace CycleEquivalence

using Node = CycleEquivalence::Node;
using Edge = CycleEquivalence::Edge;
struct RegionBoundary {
  Edge const * start;
  Edge const * end;
};
struct Region {
  /* NOTE(jordan): a Block SESE Region encloses a single BasicBlock. A
   * Region SESE Region encloses other Regions.
   */
  enum class EnclosesType { Block, Region };
  /* NOTE(jordan): a Canonical SESE Region is the largest, closest
   * region encapsulating a set of basic blocks. Canonical Regions are
   * derived from the CycleEquivalence::Graph.
   *
   * A NonCanonical SESE Region may be split out of a Canonical Region,
   * or it may combine Canonical Regions at the same depth, to describe
   * a program region as it relates to Canonical Regions.
   */
  enum class StructureType { Canonical, NonCanonical };
  int depth = -1;
  EnclosesType enclosesType;
  StructureType structureType;
  Region * parent = nullptr;
  // NOTE(jordan): only non-root Regions have start & end Edges
  Edge const * start = nullptr;
  Edge const * end   = nullptr;


  // NOTE(jordan): only a Region of EnclosesType::Region has children.
  std::vector<Region *> children = {};
  // NOTE(jordan): only a Region of EnclosesType::Block has a block.
  BasicBlock * block = nullptr;
  // NOTE(jordan): every Region has a (possibly empty) annotation.
  Note::Annotation annotation = {};

  /* Basic blocks contained in regions under this one, or just the
   * basic block if enclosesType == Block, for convenience/testing
   */
  std::vector<BasicBlock *> basic_block_offspring = {};

  /* Same as above but with regions
   */
  std::vector<Region *> region_offspring = {};

  /*
   * Get all regions that are offspring of this region
   */
  void getAllOffspringRegions( void );

};
struct Tree {
  bool valid;
  bool empty;
  Region const * root;
  std::vector<Region> regions;
  std::map<Region const *, Note::Annotation> annotations;
  static Tree compute (CycleEquivalence::Graph const &);

  // NOTE(jordan): traversal methods.
  Region * innermostRegionForBlock (BasicBlock const *);
  Region * out (Region const &);
  Region * in  (Region const &);
  Region * out_canonical (Region const &);
  Region * in_canonical  (Region const &);

  // NOTE(jordan): annotation getter/setters.
  using string = std::string;
  bool hasAnnotation (string key, Region const *r) { return r->annotation.find(key) != r->annotation.end(); }
  std::string getAnnotation (string, Region const *);
  void upsertAnnotation (string, string, Region const *);

  /*
   * Return the first common ancestor, null if root
   */
  Region *getFirstCommonAncestor(Region *r1, Region *r2);

  /*
   * If the basic blocks of i1 and i2 share an ancestor with this annotation
   * then it is valid, otherwise not
   */
  bool validAnnotation(llvm::Instruction *i1, llvm::Instruction *i2, string a);

  /*
   *
   */
  std::vector<Region *> getAllLeafOffspring(Region *r);
};
}

namespace llvm {

  struct TalkDown : public ModulePass {
    public:
      static char ID;

      TalkDown () : ModulePass{ ID } {}

      bool doInitialization (Module &M) override;
      bool runOnModule (Module &M) override;
      void getAnalysisUsage(AnalysisUsage &AU) const override;

      /* bool SESE::Tree::validAnnotation(llvm::Instruction *i1, llvm::Instruction *i2, std::string a); */
      bool validAnnotation(Instruction *i1, Instruction *i2, std::string a) {
        return sese_tree.validAnnotation(i1, i2, a);
      }

    private:
      // TODO(jordan): coalesce BasicBlocks into an SESE tree.
      SESE::Tree sese_tree;

  };
}
