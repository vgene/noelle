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

namespace SpanningTree {
  struct Node {
    Node (BasicBlock const * block) : block(block) {}
    BasicBlock const * block;
    int index = -1; // index in spanning-tree (simplifies cycle equiv.)
    std::vector<SpanningTree::Node *> children;
    std::vector<BasicBlock const *> bb_unused_children;
    std::vector<Node const *> backedges;
  };
  struct Tree {
    bool empty = true;
    Node * root;
    std::vector<Node *> nodes; // nodes ordered by depth (root-first)
    using BackEdge = std::pair<Node *, Node *>; // NOTE: undirected
    std::vector<BackEdge> backedges; // NOTE: back-edges are unordered
  };

  SpanningTree::Tree * compute (
    llvm::Function const & function,
    std::vector<SpanningTree::Node *> & tree_vector
  );

  SpanningTree::Node * compute_recursive (
    llvm::BasicBlock const & start,
    std::vector<BasicBlock const *> & visited,
    std::vector<SpanningTree::Node *> & tree_vector
  );

  void compute_backedges (Tree & tree);

  void print (SpanningTree::Tree const & tree, llvm::raw_ostream & os);
  void print_recursive (
    SpanningTree::Node const & start,
    llvm::raw_ostream & os
  );

  void print (Tree const & tree, llvm::raw_ostream & os) {
    if (tree.empty) {
      os << "Spanning Tree is empty.\n";
      return;
    }
    os << "Nodes:\n";
    print_recursive(*tree.root, os);
    os << "Back edges:";
    if (tree.backedges.size() == 0) os << "\n\t(none)";
    for (auto const & backedge : tree.backedges) {
      os
        << "\n\tNode (" << backedge.first << ")"
        << " ↔ Node (" << backedge.second << ")";
    }
  }

  void print_recursive (
    SpanningTree::Node const & start,
    llvm::raw_ostream & os
  ) {
    os
      << "Node (" << &start << "; BB " << start.block << ")"
      << "\n\tfirst instruction:"
      << "\n\t" << *start.block->begin()
      << "\n\tchildren:";
    if (start.children.size() == 0) os << "\n\t(none)";
    for (auto const & child : start.children) {
      os << "\n\t" << child;
    }
    os << "\n";
    for (auto const & child : start.children) {
      SpanningTree::print_recursive(*child, os);
    }
  }

  SpanningTree::Tree compute (llvm::Function const & function) {
    Tree tree = {};
    if (function.begin() == function.end()) {
      return std::move(tree);
    }
    std::vector<BasicBlock const *> visited = {};
    tree.root = SpanningTree::compute_recursive(
      *function.begin(),
      visited,
      tree.nodes
    );
    SpanningTree::compute_backedges(tree);
    tree.empty = false;
    return std::move(tree);
  }

  SpanningTree::Node * compute_recursive (
    llvm::BasicBlock const & start,
    std::vector<BasicBlock const *> & visited,
    std::vector<SpanningTree::Node *> & tree_vector
  ) {
    // Construct node for this block
    SpanningTree::Node * node = new SpanningTree::Node(&start);
    tree_vector.push_back(node);
    node->index = (tree_vector.size() - 1);
    // Drain successors iterator into a vector
    auto succ_it = llvm::successors(&start);
    std::vector<BasicBlock const *> successors;
    for (auto const & succ : succ_it) { successors.push_back(succ); }
    // Visit this node (to prevent successors from looping back to it)
    visited.push_back(&start);
    // Reach not-yet-visited children, add back-edges for visited children
    for (auto & succ : successors) {
      auto visited_succ = std::find(visited.begin(), visited.end(), succ);
      if (visited_succ == visited.end()) {
        node->children.push_back(
          SpanningTree::compute_recursive(*succ, visited, tree_vector)
        );
      } else {
        node->bb_unused_children.push_back(succ);
      }
    }
    return node;
  }

  void compute_backedges (SpanningTree::Tree & tree) {
    for (auto & node : tree.nodes) {
      for (auto & bb_backedge : node->bb_unused_children) {
        SpanningTree::Node * reached_node = nullptr;
        for (auto & seek_node : tree.nodes) {
          if (seek_node->block == bb_backedge) {
            reached_node = seek_node;
          }
        }
        assert(
          reached_node != nullptr
          && "back-edge is not in tree?"
        );
        node->backedges.push_back(reached_node);
        reached_node->backedges.push_back(node);
        tree.backedges.push_back(std::make_pair(node, reached_node));
      }
    }
  }
}

namespace SESE {
namespace CycleEquivalence {
  struct BracketList; // NOTE(jordan): forward declarations for Edge.
  struct Node;
  struct Edge { // The Program Structure Tree. Section 3.5. p177
    int cycle_class;  // index of cycle equivalence class
    int recent_size;  // size of bracket set when this was top edge
    int recent_class; // equiv. class no. of *tree* edge where this was
                      //   most recently the topmost bracket
    std::pair<Node const *, Node const *> ends;
    bool touches (Node const *);
  };
  struct BracketList { // The Program Structure Tree. Section 3.5. p177
    std::vector<Edge const *> brackets;
    // create() : BracketList
    BracketList () {}
    // size (bl: BracketList) : integer
    int size () { return brackets.size(); }
    // push (bl: BracketList, e: bracket): BracketList
    void push (Edge const & edge) {
      brackets.push_back(&edge);
    }
    // top (bl: BracketList) : bracket
    Edge const * top () { return brackets.back(); }
    // delete (bl: BracketList, e: bracket) : BracketList
    void del (Edge const & edge) {
      std::remove(std::begin(brackets), std::end(brackets), &edge);
    }
    // concat (bl1, bl2: BracketList) : BracketList
    void concat (BracketList const & other) {
      brackets.reserve(brackets.size() + other.brackets.size());
      for (auto other_bracket : other.brackets) {
        brackets.push_back(other_bracket);
      }
    }
    /*
    static BracketList create ();
    static int size (BracketList const & list);
    static BracketList push (Edge & bracket, BracketList const & list);
    static Edge const * top (BracketList const & list);
    static BracketList del (Edge & bracket, BracketList const & list);
    static BracketList concat (BracketList const &, BracketList const &);
    */
  };
  struct Node { // The Program Structure Tree. Section 3.5. p177
    int hi;        // dfs_index of dest. nearest to root from a descndnt
    int dfs_index; // depth-first search index of node
    BracketList bracket_list;
    std::vector<int> children;   // referenced by dfs-index
    std::vector<int> backedges; // referenced by dfs-index
    SpanningTree::Node const * in_spanning_tree;
    static bool compare_hi (Node const &, Node const &);
  };
  struct Graph {
    std::vector<Node> nodes;
    static bool descends_from (Node const &, Node const &);
  };

  bool Node::compare_hi (Node const & a, Node const & b) {
    return a.hi < b.hi;
  }

  bool Edge::touches (Node const * node) {
    return this->ends.first == node || this->ends.second == node;
  }

  Graph from_spanning_tree (SpanningTree::Tree const & tree) {
    // 1. Add all nodes to the graph.
    std::vector<Node> nodes;
    for (int dfs_ix = (tree.nodes.size() - 1); dfs_ix >= 0; dfs_ix--) {
      auto const * tree_node = tree.nodes.at(dfs_ix);
      // 1.a. compute children indices
      std::vector<int> children;
      for (auto const * tree_child : tree_node->children) {
        children.push_back(tree_child->index);
      }
      // 1.b. compute back-edge indices
      std::vector<int> backedges;
      for (auto const * tree_backedge : tree_node->backedges) {
        backedges.push_back(tree_backedge->index);
      }
      // 1.c. push node onto graph
      nodes.push_back({
        /* hi           */ -1,
        /* dfs_index    */ dfs_ix,
        /* bracket_list */ {},
        /* children     */ std::move(children),
        /* backedges   */ std::move(backedges),
        /* in_spanning_tree */ tree_node,
      });
    }
    // 2. Compute pointer graphs for children, back-edges.
    std::map<Node const *, std::vector<Node const *>> child_graph;
    std::map<Node const *, std::vector<Node const *>> backedge_graph;
    for (auto & node : nodes) {
      std::vector<Node const *> children = {};
      std::vector<Node const *> backedges = {};
      for (auto & child_index : node.children) {
        children.push_back(&nodes.at(child_index));
      }
      for (auto & backedge_index : node.backedges) {
        backedges.push_back(&nodes.at(backedge_index));
      }
      child_graph.insert({ &node, std::move(children) });
      backedge_graph.insert({ &node, std::move(backedges) });
    }
    // 3. Calculate cycle equivalence.
    // The Program Structure Tree. Section 3.5. Figure 4. p178
    // "for each node in reverse depth-first order"
    for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
      Node & node = *it;
      // "compute n.hi"
      // hi0 = min({ t.dfsnum | (n, t) is a back-edge })
      auto backedge_pointers = backedge_graph.at(&node);
      auto hi0_node_it = std::min_element(
        std::begin(backedge_pointers),
        std::end(backedge_pointers),
        Node::compare_hi
      );
      int hi0 = (*hi0_node_it)->hi;
      // hi1 = min({ c.hi | c is a child of n })
      auto child_pointers = child_graph.at(&node);
      auto hi1_node_it = std::min_element(
        std::begin(child_pointers),
        std::end(child_pointers),
        Node::compare_hi
      );
      int hi1 = (*hi1_node_it)->hi;
      // n.hi = min({ hi0, hi1 })
      node.hi = std::min(hi0, hi1);
      // hichild = any child c of n having c.hi = hi1
      // hi2 = min({ c.hi | c is a child of n other than hichild })
      auto lo_children = child_pointers;
      std::remove_if(
        std::begin(lo_children),
        std::end(lo_children),
        [&] (Node const * child) { return child->hi == hi1; }
      );
      auto hi2_node_it = std::min_element(
        std::begin(lo_children),
        std::end(lo_children),
        Node::compare_hi
      );
      int hi2 = (*hi2_node_it)->hi;

      // "compute bracketlist"
      std::map<Node const *, std::vector<Node const *>> capping_backedges;
      // for each child c of n do
      for (auto child_pointer : child_pointers) {
        // n.blist = concat(c.blist, n.blist)
        node.bracket_list = BracketList::concat(
          node.bracket_list,
          child_pointer->bracket_list
        );
      }
      // for each capping backedge d from a descendant of n to n do
      for (auto capping_backedge : capping_backedges.at(&node)) {
        // delete(n.blist, d)
        if (Graph::descends_from(node, *capping_backedge)) {
          BracketList::del(capping_backedge, node.bracket_list);
        }
      }
      // for each backedge b from a descendant of n to n do
      for (/* ??? */) {
        // delete(n.blist, b)
        // if b.class undefined then: b.class = new-class()
      }
      // for each backedge e from n to an ancestor of n do
      for (/* ??? */) {
        // push(n.blist, e)
      }
      // if hi2 < hi0:
      if (hi2 < hi0) {
        // "create capping backedge"
        // d = (n, node[hi1])
        // push(n.blist, d)
      }

      // "determine class for edge from parent(n) to n */
      // if n is not the root of dfs tree:
      if (node.in_spanning_tree != tree.root) {
        // let e be the tree edge from parent(n) to n
        // b = top(n.blist)
        // if b.recentSize != size(n.blist) then
          // b.recentSize = size(n.blist)
          // b.recentClass = new-class()
        // e.class = b.recentClass

        // "check for e, b equivalences"
        // if b.recentSize = 1 then
          // b.class = e.class
      }
    }
  }
} // namespace: CycleEquivalenceGraph
} // namespace: SESE

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
        << "\t"  << annotation_entry.first
        << " = " << annotation_entry.second
        << "\n";
    }
    os << "};";
  }
}

bool llvm::TalkDown::doInitialization (Module &M) {
  return false;
}

// TODO(jordan): it looks like this can be refactored as a FunctionPass
bool llvm::TalkDown::runOnModule (Module &M) {
  /* 1. Split BasicBlocks wherever the applicable annotation changes
   * 2. Construct SESE tree at BasicBlock granularity; write query APIs
   */

  using SplitPoint = Instruction *;
  llvm::SmallVector<SplitPoint, 8> splits;

  // Collect all the split points in each function
  // TODO(jordan): refactor this into its own function.
  for (auto & function : M) {
    MDNode * last_note_meta = nullptr;
    for (auto & block : function) {
      for (auto & inst : block) {
        /* NOTE(jordan): When there's a new annotation (or none, when
         * there was one; or one, where there was none), we should parse
         * and capture the annotation.
         */
        bool hasMeta = inst.hasMetadata();
        if (false
          || (!hasMeta && last_note_meta != nullptr)
          || (inst.getMetadata("note.noelle") != last_note_meta)
        ) {
          // NOTE(jordan): also split if it changes within a block.
          if (true                                 // Split if...
            && (&inst != &*block.begin())          // not the first
            && (&inst != &*std::prev(block.end())) // or last.
          ) {
            splits.emplace_back(&inst);
          }
          // NOTE(jordan): DEBUG
          llvm::errs()
            << "\nInstruction where annotation changes w/in a block:"
            << "\n\t" << inst
            << "\n";
          // NOTE(jordan): always save the annotation, even if no split
          if (hasMeta) {
            last_note_meta = inst.getMetadata("note.noelle");
            // NOTE(jordan): DEBUG
            Annotation note = Note::parse_metadata(last_note_meta);
            Note::print_annotation(note, llvm::errs());
            llvm::errs() << "\n";
          } else {
            // NOTE(jordan): DEBUG
            llvm::errs() << "Annotation (none ‒ unset)\n";
            last_note_meta = nullptr;
          }
        }
      }
    }
  }

  llvm::errs() << "\nSplit points constructed: " << splits.size() << "\n";

  // Perform splitting
  for (SplitPoint & split : splits) {
    llvm::errs()
      << "Split:"
      << "\n\tin block @ " << split->getParent()
      << "\n\tbefore instruction @ " << split
      << "\n\t" << *split
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
  llvm::errs() << "\n";
  for (auto & function : M) {
    SpanningTree::Tree tree = SpanningTree::compute(function);
    llvm::errs() << "Spanning Tree for " << function.getName() << "\n";
    if (tree.empty) {
      // NOTE(jordan): DEBUG
      llvm::errs() << "(empty spanning tree)";
    } else {
      SpanningTree::print(tree, llvm::errs());
    }
    llvm::errs() << "\n\n";
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
