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
}

namespace SESE {
namespace UndirectedCFG {
  struct Node {
    BasicBlock const * block;
  };
  using Edge = SESE::AbstractEdge<Node>;
  struct Graph {
    bool empty = true;
    std::vector<Node> const nodes;
    std::set<Edge> const edges;
    std::map<Node const *, std::set<Edge const *>> const edge_map;
  };

  Graph compute (llvm::Function const & function) {
    if (function.begin() == function.end()) {
      return {};
    }
    std::vector<Node> nodes;
    for (llvm::BasicBlock const & block : function) {
      nodes.push_back({ &block });
    }
    std::map<BasicBlock const *, Node const *> block_to_node;
    // NOTE(jordan): for... reasons... this cannot be in the above loop.
    for (Node const & node : nodes) {
      block_to_node.insert({ node.block, &node });
    }
    std::set<Edge> edges;
    std::map<Node const *, std::set<Edge const *>> edge_map;
    for (Node const & node : nodes) {
      auto succ_it = llvm::successors(node.block);
      std::set<Node const *> next;
      for (auto succ : succ_it) next.insert(block_to_node.at(succ));
      for (Node const * other : next) {
        edges.insert({ &node, other });
      }
    }
    for (Edge const & edge : edges) {
      auto node  = edge.first;
      auto other = edge.second;
      if (edge_map.find(node) == edge_map.end()) {
        edge_map.insert({ node, {} });
      }
      if (edge_map.find(other) == edge_map.end()) {
        edge_map.insert({ other, {} });
      }
      edge_map.at(node).insert(&edge);
      edge_map.at(other).insert(&edge);
    }
    return {
      (edges.size() == 0),
      std::move(nodes),
      std::move(edges),
      std::move(edge_map)
    };
  }

  void print (Graph const & graph, llvm::raw_ostream & os) {
    for (auto const & entry : graph.edges) {
      auto const & a = entry.first;
      auto const & b = entry.second;
      os << "Edge:";
      os << "\n\tNode (" << a << "; BB " << a->block << ")";
      os << "\n\tNode (" << b << "; BB " << b->block << ")";
      os << "\n";
    }
  }
} // namespace UndirectedCFG
} // namespace SESE

namespace SESE {
namespace SpanningTree {
  struct Node {
    Node (int dfs_index, BasicBlock const * block, Node const * parent)
      : dfs_index(dfs_index), block(block), parent(parent)
      {}
    BasicBlock const * block;
    int dfs_index; // index in tree (simplifies cycle equiv.)
    Node const * parent;
    std::vector<Node const *> children;
    std::vector<Node const *> backedges;
    std::vector<BasicBlock const *> bb_unused_children;
  };
  struct Tree {
    bool empty = true;
    Node const * root;
    std::vector<Node *> nodes; // nodes ordered by visitation order
    using BackEdge = std::pair<Node *, Node *>; // NOTE: undirected
    std::vector<BackEdge> backedges; // NOTE: back-edges are unordered
  };

  SpanningTree::Tree * compute (
    UndirectedCFG::Graph const &,
    std::vector<SpanningTree::Node *> &
  );

  SpanningTree::Node * compute_recursive (
    UndirectedCFG::Graph const &,
    UndirectedCFG::Node const &,
    std::vector<BasicBlock const *> &,
    std::vector<SpanningTree::Node *> &
  );

  void compute_backedges (Tree &);

  void print (SpanningTree::Tree const &, llvm::raw_ostream &);
  void print_recursive (
    SpanningTree::Node const &,
    llvm::raw_ostream &
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

  SpanningTree::Tree compute (UndirectedCFG::Graph const & graph) {
    Tree tree = {};
    if (graph.empty) {
      return std::move(tree);
    }
    std::vector<BasicBlock const *> visited = {};
    tree.root = SpanningTree::compute_recursive(
      graph,
      graph.nodes.front(),
      visited,
      tree.nodes
    );
    SpanningTree::compute_backedges(tree);
    tree.empty = false;
    return std::move(tree);
  }

  SpanningTree::Node * compute_recursive (
    UndirectedCFG::Graph const & graph,
    UndirectedCFG::Node const & start,
    std::vector<BasicBlock const *> & visited,
    std::vector<SpanningTree::Node *> & tree_vector,
    Node const * parent = nullptr
  ) {
    // Construct node for this block
    SpanningTree::Node * node = new SpanningTree::Node(
      tree_vector.size(),
      start.block,
      parent
    );
    tree_vector.push_back(node);
    // Visit this node (to prevent next nodes from looping back to it)
    visited.push_back(node->block);
    // Reach not-yet-visited children, add back-edges for visited children
    for (auto edge : graph.edges) {
      if (!edge.touches(&start)) {
        // This edge is not one of ours
        continue;
      }
      auto next_result = edge.other_end(&start);
      assert(next_result.first && "Edge touches but has no other end?");
      auto next = next_result.second;
      auto visited_next = std::find(
        visited.begin(),
        visited.end(),
        next->block
      );
      if (visited_next != visited.end()) {
        node->bb_unused_children.push_back(next->block);
      } else {
        node->children.push_back(
          SpanningTree::compute_recursive(
            graph,
            *next,
            visited,
            tree_vector,
            node
          )
        );
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
        // NOTE(jordan): the back-edge cannot be a child edge reversed
        auto reached_as_node_child = std::find(
          node->children.begin(),
          node->children.end(),
          reached_node
        );
        if (reached_as_node_child != node->children.end()) {
          // reached is a child of node; false backedge
          continue;
        }
        auto node_as_reached_child = std::find(
          reached_node->children.begin(),
          reached_node->children.end(),
          node
        );
        if (node_as_reached_child != reached_node->children.end()) {
          // node is a child of reached; false backedge
          continue;
        }
        // NOTE(jordan): only add the backedge if it's unique
        auto existing_backedge = std::find(
          node->backedges.begin(),
          node->backedges.end(),
          reached_node
        );
        if (existing_backedge != node->backedges.end()) {
          // backedge has already been found
          continue;
        }
        tree.backedges.push_back(std::make_pair(node, reached_node));
        node->backedges.push_back(reached_node);
        reached_node->backedges.push_back(node);
      }
    }
  }
} // namespace SpanningTree
} // namespace SESE

namespace SESE {
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
    /* NOTE(jordan):
     * According to PST paper, the Edge should contain a pointer to its
     * BracketList node; however, since we are using a vector, the
     * equivalent is to store the index.
     */
    int bracket_list_index = -1;
    Edge (Node const * a, Node const * b) : EdgeBase(a, b) {};
  };
  struct BracketList { // The Program Structure Tree. Section 3.5. p177
    std::vector<Edge *> brackets;
    // create() : BracketList
    BracketList () {}
    BracketList (std::vector<Edge *> bs) : brackets(bs) {}
    // size (bl: BracketList) : integer
    int size () { return brackets.size(); }
    // push (bl: BracketList, e: bracket): BracketList
    void push (Edge * edge) {
      brackets.push_back(edge);
    }
    // top (bl: BracketList) : bracket
    Edge * top () { return brackets.back(); }
    // delete (bl: BracketList, e: bracket) : BracketList
    void del (Edge const * edge) {
      std::remove(std::begin(brackets), std::end(brackets), &edge);
    }
    // concat (bl1, bl2: BracketList) : BracketList
    void concat (BracketList const & other) {
      brackets.reserve(brackets.size() + other.brackets.size());
      for (auto other_bracket : other.brackets) {
        brackets.push_back(other_bracket);
      }
    }
  };
  struct Node { // The Program Structure Tree. Section 3.5. p177
    int hi;        // dfs_index of dest. nearest to root from a descndnt
    int dfs_index; // depth-first search index of node ("dfsnum")
    BracketList bracket_list;
    std::vector<int> children;  // referenced by dfs-index
    std::vector<int> backedges; // referenced by dfs-index
    int parent;
  };
  struct Graph {
    std::vector<Node> nodes;
    std::map<Node const *, std::vector<Edge>> tree_edges;
    std::map<Node const *, std::vector<Edge>> backedges;
    std::map<Node const *, std::vector<Edge>> capping_backedges;
    static bool descends_from (Node const &, Node const &);
  };

  Graph from_spanning_tree (SpanningTree::Tree const & tree) {
    // 1. Add all nodes to the graph.
    std::vector<Node> nodes;
    for (int dfs_ix = (tree.nodes.size() - 1); dfs_ix >= 0; dfs_ix--) {
      auto const * tree_node = tree.nodes.at(dfs_ix);
      // 1.a. compute children indices
      std::vector<int> children;
      for (auto const * tree_child : tree_node->children) {
        children.push_back(tree_child->dfs_index);
      }
      // 1.b. compute back-edge indices
      std::vector<int> backedges;
      for (auto const * tree_backedge_node : tree_node->backedges) {
        backedges.push_back(tree_backedge_node->dfs_index);
      }
      // 1.c. push node onto graph
      nodes.push_back({
        /* hi           */ -1,
        /* dfs_index    */ dfs_ix,
        /* bracket_list */ {},
        /* children     */ std::move(children),
        /* backedges    */ std::move(backedges),
        /* parent       */ tree_node->parent->dfs_index,
      });
    }
    // 2. Construct rich edge pointer graphs for children, back-edges.
    std::map<Node const *, std::vector<Edge>> tree_graph;
    std::map<Node const *, std::vector<Edge>> backedge_graph;
    for (auto & node : nodes) {
      std::vector<Edge> tree_edges = {};
      std::vector<Edge> backedges  = {};
      for (auto & child_index : node.children) {
        auto child = nodes.at(child_index);
        tree_edges.push_back({ &node, &child });
      }
      for (auto & backedge_index : node.backedges) {
        auto dest = nodes.at(backedge_index);
        backedges.push_back({ &node, &dest });
      }
      tree_graph.insert({ &node, std::move(tree_edges) });
      backedge_graph.insert({ &node, std::move(backedges) });
    }
    // 3. Calculate cycle equivalence.
    // The Program Structure Tree. Section 3.5. Figure 4. p178
    // "for each node in reverse depth-first order"
    std::map<Node const *, std::vector<Edge>> capping_backedges;
    for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
      Node & node = *it;
      /* NOTE(jordan): preallocate a capping_backedges vector large enough
       * we are confident it will not get resized. Otherwise, it's just...
       * ugh. It gets annoying to try to keep track of memory if the
       * vector resizes.
       */
      std::vector<Edge> cap_edges = {};
      cap_edges.reserve(node.children.size());
      capping_backedges.insert({ &node, std::move(cap_edges) });

      // "compute n.hi"
      // hi0 = min({ t.dfsnum | (n, t) is a back-edge })
      auto hi0_dfsnum_it = std::min_element(
        std::begin(node.backedges),
        std::end(node.backedges)
      );
      int hi0 = INT_MAX;
      if (hi0_dfsnum_it != std::end(node.backedges)) {
        hi0 = *hi0_dfsnum_it;
      }
      // hi1 = min({ c.hi | c is a child of n })
      auto hi1_dfsnum_it = std::min_element(
        std::begin(node.children),
        std::end(node.children),
        [&] (int a_dfs_index, int b_dfs_index) {
          return nodes.at(a_dfs_index).hi < nodes.at(b_dfs_index).hi;
        }
      );
      int hi1 = INT_MAX;
      if (hi1_dfsnum_it != std::end(node.children)) {
        hi1 = nodes.at(*hi1_dfsnum_it).hi;
      }
      // n.hi = min({ hi0, hi1 })
      node.hi = std::min(hi0, hi1);
      // hichild = any child c of n having c.hi = hi1
      // hi2 = min({ c.hi | c is a child of n other than hichild })
      auto lo_children = node.children;
      std::remove_if(
        std::begin(lo_children),
        std::end(lo_children),
        [&] (int child_dfs_index) {
          return nodes.at(child_dfs_index).hi == hi1;
        }
      );
      auto hi2_dfsnum_it = std::min_element(
        std::begin(lo_children),
        std::end(lo_children),
        [&] (int a_dfs_index, int b_dfs_index) {
          return nodes.at(a_dfs_index).hi < nodes.at(b_dfs_index).hi;
        }
      );
      int hi2 = INT_MAX;
      if (hi2_dfsnum_it != std::end(lo_children)) {
        hi2 = nodes.at(*hi2_dfsnum_it).hi;
      }

      // "compute bracketlist"
      // for each child c of n do
      for (auto child_dfs_index : node.children) {
        // n.blist = concat(c.blist, n.blist)
        auto child = nodes.at(child_dfs_index);
        node.bracket_list.concat(child.bracket_list);
      }
      // for each capping backedge d from a descendant of n to n do
      for (auto const & capping_backedge : capping_backedges.at(&node)) {
        // delete(n.blist, d)
        auto other_result = capping_backedge.other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (Graph::descends_from(node, *other)) {
          //BracketList::del(capping_backedge, node.bracket_list);
          // TODO(jordan): descendancy relation
        }
      }
      // for each backedge b from a descendant of n to n do
      for (auto & backedge : backedge_graph.at(&node)) {
        auto other_result = backedge.other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (Graph::descends_from(node, *other)) {
          // TODO(jordan): descendancy relation
          // delete(n.blist, b)
          // if b.class undefined then: b.class = new-class()
        }
      }
      // for each backedge e from n to an ancestor of n do
      for (auto const & backedge : backedge_graph.at(&node)) {
        auto other_result = backedge.other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (Graph::descends_from(*other, node)) {
          // TODO(jordan): descendancy relation
          // push(n.blist, e)
        }
      }
      // if hi2 < hi0:
      if (hi2 < hi0) {
        // "create capping backedge"
        // d = (n, node[hi1])
        auto hi1_node = nodes.at(*hi1_dfsnum_it);
        // FIXME(jordan): this will probably get deallocated too soon.
        capping_backedges.at(&node).push_back({ &node, &hi1_node });
        // push(n.blist, d)
        node.bracket_list.push(&capping_backedges.at(&node).back());
      }

      // "determine class for edge from parent(n) to n */
      // if n is not the root of dfs tree:
      // NOTE(jordan): the root has dfs_index 0
      if (node.parent != 0) {
        // let e be the tree edge from parent(n) to n
        auto const & parent = nodes.at(node.parent);
        auto & parent_edges = tree_graph.at(&parent);
        auto parent_edge_it = std::find(
          std::begin(parent_edges),
          std::end(parent_edges),
          [&] (Edge const & edge) { edge.touches(&node); }
        );
        Edge & parent_edge = *parent_edge_it;
        // b = top(n.blist)
        Edge * bracket = node.bracket_list.top();
        // if b.recentSize != size(n.blist) then
        if (bracket->recent_size != node.bracket_list.size()) {
          // b.recentSize = size(n.blist)
          bracket->recent_size = node.bracket_list.size();
          // b.recentClass = new-class()
          bracket->recent_class = -1/* ??? */;
        }
        // e.class = b.recentClass
        parent_edge.cycle_class = bracket->recent_class;

        // "check for e, b equivalences"
        // if b.recentSize = 1 then
        if (bracket->recent_size == 1) {
          // b.class = e.class
          bracket->cycle_class = parent_edge.cycle_class;
        }
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
    namespace SpanningTree = SESE::SpanningTree;
    auto undirected_cfg = SESE::UndirectedCFG::compute(function);
    llvm::errs() << "Undirected CFG for " << function.getName() << "\n";
    if (undirected_cfg.empty) {
      llvm::errs() << "(graph is empty)";
    } else {
      SESE::UndirectedCFG::print(undirected_cfg, llvm::errs());
    }
    llvm::errs() << "\n";
    llvm::errs() << "Spanning Tree for " << function.getName() << "\n";
    SpanningTree::Tree tree = SpanningTree::compute(undirected_cfg);
    if (tree.empty) {
      // NOTE(jordan): DEBUG
      llvm::errs() << "(spanning tree is empty)";
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
