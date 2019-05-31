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
#include "llvm/IR/CFG.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"

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
    bool valid;
    bool empty;
    Node const * exit;
    std::vector<Node> const nodes;
    std::vector<Edge> const edges;
  };

  Graph compute (llvm::Function const & function) {
    if (std::begin(function) == std::end(function)) {
      return { /* valid */ true, /* empty */ true };
    }
    std::vector<Node> nodes;
    for (llvm::BasicBlock const & block : function) {
      // NOTE(jordan): do not handle unreachable blocks.
      if (true
        && &block != &function.getEntryBlock()
        && llvm::pred_begin(&block) == llvm::pred_end(&block)
      ) {
        llvm::errs() << "Unreachable block found! Aborting...\n";
        return { /* valid */ false };
      }
      nodes.push_back({ &block });
    }
    /* NOTE(jordan): because the resizing of the vector causes pointers to
     * change, this loop cannot be combined with the above loop.
     */
    std::map<BasicBlock const *, Node const *> block_to_node;
    for (Node const & node : nodes) {
      block_to_node.insert({ node.block, &node });
    }
    Node const * exit = nullptr;
    std::vector<Edge> edges;
    for (Node const & node : nodes) {
      auto succ_it = llvm::successors(node.block);
      std::vector<Node const *> next;
      for (auto succ : succ_it) next.push_back(block_to_node.at(succ));
      if (next.size() == 0) exit = &node;
      for (Node const * other : next) {
        edges.push_back({ &node, other });
      }
    }
    assert(exit != nullptr);
    return {
      /* valid */ true,
      /* empty */ (edges.size() == 0),
      /* exit  */ std::move(exit),
      /* nodes */ std::move(nodes),
      /* edges */ std::move(edges),
    };
  }

  void print (Graph const & graph, llvm::raw_ostream & os) {
    assert(graph.valid);
    for (auto const & entry : graph.edges) {
      auto const & a = entry.first;
      auto const & b = entry.second;
      os
        << "\nEdge:"
        << "\n\tNode (" << a << "; BB " << a->block << ")"
        << "\n\tNode (" << b << "; BB " << b->block << ")";
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
    int dfs_index; // index in tree (simplifies cycle equiv.)
    BasicBlock const * block;
    Node const * parent;
    std::vector<Node const *> children;
    std::vector<Node const *> backedges;
    std::vector<BasicBlock const *> bb_unused_children;
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

  Tree compute (
    UndirectedCFG::Graph const &,
    std::vector<Node *> &
  );

  Node * compute_recursive (
    UndirectedCFG::Graph const &,
    UndirectedCFG::Node const &,
    std::vector<BasicBlock const *> &,
    std::vector<Node *> &,
    Node const * parent
  );

  std::vector<Backedge> compute_backedges (
    std::vector<Node *> const &
  );

  Node const * identify_exit (
    std::vector<Node *> const &,
    UndirectedCFG::Node const *
  );

  void print (Tree const &, llvm::raw_ostream &);
  void print_recursive (Node const &, llvm::raw_ostream &);

  void print (Tree const & tree, llvm::raw_ostream & os) {
    assert(tree.valid);
    print_recursive(*tree.root, os);
    os << "\nBack edges:";
    if (tree.backedges.size() == 0) os << "\n\t(none)";
    for (auto const & backedge : tree.backedges) {
      os
        << "\n\tNode (" << backedge.first << ")"
        << " ↔ Node (" << backedge.second << ")";
    }
  }

  void print_recursive (Node const & start, llvm::raw_ostream & os) {
    os
      << "\nNode (" << &start << "; BB " << start.block << ")"
      << "\n\tfirst instruction:"
      << "\n\t" << *start.block->begin()
      << "\n\tchildren:";
    if (start.children.size() == 0) os << "\n\t(none)";
    for (auto const & child : start.children) {
      os << "\n\t" << child;
    }
    for (auto const & child : start.children) {
      SpanningTree::print_recursive(*child, os);
    }
  }

  Tree compute (UndirectedCFG::Graph const & graph) {
    if (!graph.valid) {
      return { /* valid */ false };
    }
    if (graph.empty) {
      return { /* valid */ true, /* empty */ true };
    }
    std::vector<Node *> nodes;
    std::vector<BasicBlock const *> bb_visited = {};
    Node * root = SpanningTree::compute_recursive(
      /* graph        */ graph,
      /* start        */ graph.nodes.front(),
      /* bb_visited   */ bb_visited,
      /* tree_vector  */ nodes,
      /* parent       */ nullptr
    );
    auto backedges = SpanningTree::compute_backedges(nodes);
    auto exit      = SpanningTree::identify_exit(nodes, graph.exit);
    return {
      /* valid */ true,
      /* empty */ false,
      std::move(root),
      std::move(exit),
      std::move(nodes),
      std::move(backedges)
    };
  }

  Node * compute_recursive (
    UndirectedCFG::Graph const & graph,
    UndirectedCFG::Node const & start,
    std::vector<BasicBlock const *> & bb_visited,
    std::vector<Node *> & tree_vector,
    Node const * parent
  ) {
    // Construct node for this block
    Node * node = new SpanningTree::Node(
      tree_vector.size(),
      start.block,
      parent
    );
    tree_vector.push_back(node);
    // Visit this node (to prevent next nodes from looping back to it)
    bb_visited.push_back(node->block);
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
        std::begin(bb_visited),
        std::end(bb_visited),
        next->block
      );
      if (visited_next != bb_visited.end()) {
        node->bb_unused_children.push_back(next->block);
      } else {
        node->children.push_back(
          SpanningTree::compute_recursive(
            graph,
            *next,
            bb_visited,
            tree_vector,
            node
          )
        );
      }
    }
    return node;
  }

  std::vector<Backedge> compute_backedges (
    std::vector<Node *> const & nodes
  ) {
    std::vector<Backedge> backedges = {};
    for (auto & node : nodes) {
      for (auto & bb_backedge : node->bb_unused_children) {
        Node * reached_node = nullptr;
        for (auto & seek_node : nodes) {
          if (seek_node->block == bb_backedge) {
            reached_node = seek_node;
          }
        }
        assert(
          reached_node != nullptr
          && "back-edge endpoint is not in tree"
        );
        // NOTE(jordan): the back-edge cannot be a child edge reversed
        auto reached_as_node_child = std::find(
          std::begin(node->children),
          std::end(node->children),
          reached_node
        );
        if (reached_as_node_child != node->children.end()) {
          // reached is a child of node; false backedge
          continue;
        }
        auto node_as_reached_child = std::find(
          std::begin(reached_node->children),
          std::end(reached_node->children),
          node
        );
        if (node_as_reached_child != reached_node->children.end()) {
          // node is a child of reached; false backedge
          continue;
        }
        // NOTE(jordan): only add the backedge if it's unique
        auto existing_backedge = std::find_if(
          std::begin(backedges),
          std::end(backedges),
          [&] (Backedge const & backedge) {
            return true
              && backedge.touches(node)
              && backedge.touches(reached_node)
              ;
          }
        );
        if (existing_backedge != std::end(backedges)) {
          // backedge has already been found
          continue;
        }
        backedges.push_back({ node, reached_node });
        node->backedges.push_back(reached_node);
      }
    }
    return std::move(backedges);
  }

  Node const * identify_exit (
    std::vector<Node *> const & nodes,
    UndirectedCFG::Node const * cfg_exit
  ) {
    return *std::find_if(
      std::begin(nodes),
      std::end(nodes),
      [&] (Node const * node) {
        return node->block == cfg_exit->block;
      }
    );
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
    BasicBlock const * block;
    bool descends_from (Node const &, std::vector<Node> const &) const;
  };
  struct Graph {
    bool valid;
    bool empty;
    Node const * start;
    Node const * exit;
    std::vector<Node> const nodes;
    std::vector<Edge> const edges;
    std::vector<Edge> const backedges;
    std::map<Node const *, std::vector<Edge *>> const tree_edges;
    std::map<Node const *, std::vector<Edge *>> const backedge_graph;
    /* NOTE(jordan): capping backedges are only applicable to unstructured
     * loops in complex control flow. Unstructured loops are theoretically
     * handled by the algorithm, but we do not support them / haven't
     * tested for them.
     */
    std::map<Node const *, std::vector<Edge>> const capping_backedges;
    static Graph compute (SpanningTree::Tree const &);
    static void print (Graph const &, llvm::raw_ostream &, bool);
    static void print_brackets (Graph const &, llvm::raw_ostream &);
  };

  bool Node::descends_from (
    Node const & ancestor,
    std::vector<Node> const & ordered_nodes
  ) const {
    if (ancestor.dfs_index == this->dfs_index) {
      return true;
    }
    return std::any_of(
      std::begin(ancestor.children),
      std::end(ancestor.children),
      [&] (int child) {
        return this->descends_from(
          ordered_nodes.at(child),
          ordered_nodes
        );
      }
    );
  }

  /* The Program Structure Tree. Figure 4.
   * The cycle equivalence algorithm.
   *
   * NOTE(jordan): While the algorithm as-presented in the paper supports
   * complex unstructured control-flow (see p176, figure 3), such programs
   * are generally regarded as "bad." (That is, they use 'goto'.) We've
   * made the decision to implement the full algorithm, including the
   * "capping backedges" that must be added to support unstructured loops,
   * but we do not test or strictly intend to support unstructured loops.
   *
   * FIXME(jordan): The below code should be adapted to abort in the
   * presence of an unstructured loop. Possibly: abort in any case where a
   * capping back-edge would otherwise be created. More test cases should
   * be added before it is decided that this is a safe choice.
   *
   * NOTE(jordan):
   * Adaptations made from the paper: rather than construct an artificial
   * start (source) and end (sink) node with an all-enclosing false
   * back-edge, we simply treat any node with 0 brackets as a "top-most"
   * node whose cycle class defaults to the special top-most class of 0.
   */
  Graph Graph::compute (SpanningTree::Tree const & tree) {
    if (!tree.valid) {
      return { /* valid */ false };
    }
    if (tree.empty) {
      return { /* valid */ true, /* empty */ true };
    }
    // 0. Start the cycle class counter
    int cycle_classes = 0;
    // 1. Add all dfs_nodes to the graph.
    std::vector<Node> dfs_nodes;
    for (int dfs_ix = 0; dfs_ix < tree.nodes.size(); dfs_ix++) {
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
      // 1.c.0 handle root parent index
      int parent_index =
        tree_node->parent != nullptr
        ? tree_node->parent->dfs_index
        : -1;
      // 1.c. push node onto graph
      dfs_nodes.push_back({
        /* hi           */ -1,
        /* dfs_index    */ dfs_ix,
        /* bracket_list */ {},
        /* children     */ std::move(children),
        /* backedges    */ std::move(backedges),
        /* parent       */ parent_index,
        /* block        */ tree_node->block,
      });
    }
    // 2. Construct rich edge pointer graphs for children, back-edges.
    std::map<Node const *, std::vector<Edge *>> tree_graph;
    std::map<Node const *, std::vector<Edge *>> backedge_graph;
    std::vector<Edge> all_backedges;
    std::vector<Edge> all_edges;
    // 2.a. Compute sets of edges and backedges & initialized graphs.
    for (auto & node : dfs_nodes) {
      for (auto & child_index : node.children) {
        Node const & child = dfs_nodes.at(child_index);
        all_edges.push_back({ &node, &child });
      }
      for (auto & backedge_index : node.backedges) {
        Node const & dest = dfs_nodes.at(backedge_index);
        all_backedges.push_back({ &node, &dest });
      }
      tree_graph.insert({ &node, {} });
      backedge_graph.insert({ &node, {} });
    }
    // 2.b. Add the artificial back-edge from the exit to the start node
    // NOTE(jordan): assumes function.begin() is the entry node
    Node & start = dfs_nodes.at(0);
    int exit_node_dfs_index = -1;
    for (auto const * tree_node : tree.nodes) {
      if (tree_node == tree.exit) {
        exit_node_dfs_index = tree_node->dfs_index;
      }
    }
    assert(exit_node_dfs_index != -1);
    Node & exit = dfs_nodes.at(exit_node_dfs_index);
    all_backedges.push_back({ &start, &exit });
    start.backedges.push_back(exit.dfs_index);
    if (&start != &exit) exit.backedges.push_back(start.dfs_index);
    // 2.c. Compute the graph of forward-edges.
    for (auto & edge : all_edges) {
      Node const & node = *edge.first;
      Node const & dest = *edge.second;
      auto & node_edges = tree_graph.at(&node);
      auto & dest_edges = tree_graph.at(&dest);
      node_edges.push_back(&edge);
      dest_edges.push_back(&edge);
    }
    // 2.d. Compute the graph of back-edges.
    for (auto & backedge : all_backedges) {
      Node const & node = *backedge.first;
      Node const & dest = *backedge.second;
      auto & node_backedges = backedge_graph.at(&node);
      auto & dest_backedges = backedge_graph.at(&dest);
      node_backedges.push_back(&backedge);
      dest_backedges.push_back(&backedge);
    }
    // 3. Calculate cycle equivalence.
    // The Program Structure Tree. Section 3.5. Figure 4. p178
    // "for each node in reverse depth-first order"
    std::map<Node const *, std::vector<Edge>> capping_backedges;
    for (auto it = dfs_nodes.rbegin(); it != dfs_nodes.rend(); it++) {
      Node & node = *it;
      // NOTE(jordan): preallocate a capping_backedges vector.
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
          return dfs_nodes.at(a_dfs_index).hi < dfs_nodes.at(b_dfs_index).hi;
        }
      );
      int hi1 = INT_MAX;
      if (hi1_dfsnum_it != std::end(node.children)) {
        hi1 = dfs_nodes.at(*hi1_dfsnum_it).hi;
      }
      // n.hi = min({ hi0, hi1 })
      node.hi = std::min(hi0, hi1);
      // hichild = any child c of n having c.hi = hi1
      // hi2 = min({ c.hi | c is a child of n other than hichild })
      auto lo_children = node.children;
      // NOTE(jordan): the idiosyncracies of C++'s std API are infinite
      auto last_removed = std::remove_if(
        std::begin(lo_children),
        std::end(lo_children),
        [&] (int child_dfs_index) {
          return dfs_nodes.at(child_dfs_index).hi == hi1;
        }
      );
      // NOTE(jordan): the idiosyncracies of C++'s std API are inscrutable
      lo_children.erase(last_removed, lo_children.end());
      // NOTE(jordan): oh C++ std, won't you please, won't you please stay
      // NOTE(jordan): oh C++ std, won't you please just stay the saaaaame
      // NOTE(jordan): baby, never change
      auto hi2_dfsnum_it = std::min_element(
        std::begin(lo_children),
        std::end(lo_children),
        [&] (int a_dfs_index, int b_dfs_index) {
          return dfs_nodes.at(a_dfs_index).hi < dfs_nodes.at(b_dfs_index).hi;
        }
      );
      int hi2 = INT_MAX;
      if (hi2_dfsnum_it != std::end(lo_children)) {
        hi2 = dfs_nodes.at(*hi2_dfsnum_it).hi;
      }
      // "compute bracketlist"
      // for each child c of n do
      for (int const child_dfs_index : node.children) {
        // n.blist = concat(c.blist, n.blist)
        Node const & child = dfs_nodes.at(child_dfs_index);
        node.bracket_list.concat(child.bracket_list);
      }
      // for each capping backedge d from a descendant of n to n do
      for (Edge const & capping_backedge : capping_backedges.at(&node)) {
        auto other_result = capping_backedge.other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (other->descends_from(node, dfs_nodes)) {
          // delete(n.blist, d);
          node.bracket_list.del(&capping_backedge);
        }
      }
      // for each backedge b from a descendant of n to n do
      for (Edge * backedge : backedge_graph.at(&node)) {
        auto other_result = backedge->other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (other->descends_from(node, dfs_nodes)) {
          // delete(n.blist, b)
          node.bracket_list.del(backedge);
          // if b.class undefined then: b.class = new-class()
          if (backedge->cycle_class == -1) {
            llvm::errs()
              << "Cycle class wasn't defined..."
              << " @ " << backedge << " class " << cycle_classes
              << "\n";
            backedge->cycle_class = cycle_classes++;
          }
        }
      }
      // for each backedge e from n to an ancestor of n do
      for (Edge * backedge : backedge_graph.at(&node)) {
        auto other_result = backedge->other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        if (node.descends_from(*other, dfs_nodes)) {
          // push(n.blist, e)
          node.bracket_list.push(backedge);
        }
      }

      // if hi2 < hi0:
      // NOTE(jordan): Only for UNSTRUCTURED loops! (untested.)
      if (hi2 < hi0) {
        assert(false && "Unstructured loops are unsupported!");
        // "create capping backedge"
        llvm::errs()
          << "\"create capping backedge\""
          << "\nNode (" << &node << " ; BB " << node.block << ")"
          << "\nhi2 " << hi2 << " hi1 " << hi1 << " hi0 " << hi0
          << "\n";
        // d = (n, node[hi1])
        auto hi1_node = dfs_nodes.at(*hi1_dfsnum_it);
        // FIXME(jordan): this will probably get deallocated too soon.
        capping_backedges.at(&node).push_back({ &node, &hi1_node });
        // push(n.blist, d)
        node.bracket_list.push(&capping_backedges.at(&node).back());
      }

      // "determine class for edge from parent(n) to n
      // if n is not the root of dfs tree:
      // NOTE(jordan): the root has a parent dfs_index of -1
      if (node.parent != -1) {
        // let e be the tree edge from parent(n) to n
        auto const & parent = dfs_nodes.at(node.parent);
        auto & parent_edges = tree_graph.at(&parent);
        auto parent_edge_it = std::find_if(
          std::begin(parent_edges),
          std::end(parent_edges),
          [&] (Edge const * edge) {
            return edge->touches(&node);
          }
        );
        assert(
          parent_edge_it != std::end(parent_edges)
          && "this node's parent has no edge to this node??"
        );
        Edge & parent_edge = **parent_edge_it;
        // b = top(n.blist)
        Edge & bracket = *node.bracket_list.top();
        // if b.recentSize != size(n.blist) then
        if (bracket.recent_size != node.bracket_list.size()) {
          // b.recentSize = size(n.blist)
          bracket.recent_size = node.bracket_list.size();
          // b.recentClass = new-class()
          bracket.recent_class = cycle_classes++;
        }
        llvm::errs()
          << "Assigning Edge (" << &parent_edge << ") class from Bracket"
          << " (" << &bracket << "): " << bracket.recent_class
          << "\n";
        // e.class = b.recentClass
        parent_edge.cycle_class = bracket.recent_class;

        // "check for e, b equivalences"
        // if b.recentSize = 1 then
        if (bracket.recent_size == 1) {
          // b.class = e.class
          bracket.cycle_class = parent_edge.cycle_class;
        }
      }
    }

    return {
      /* valid              */ true,
      /* empty              */ false,
      /* start              */ std::move(&start),
      /* exit               */ std::move(&exit),
      /* nodes              */ std::move(dfs_nodes),
      /* edges              */ std::move(all_edges),
      /* backedges          */ std::move(all_backedges),
      /* tree_edges         */ std::move(tree_graph),
      /* backedge_graph     */ std::move(backedge_graph),
      /* capping_backedges  */ std::move(capping_backedges),
    };
  }

  void Graph::print (
    Graph const & graph,
    llvm::raw_ostream & os,
    bool print_backedges = false
  ) {
    assert(graph.valid);
    for (Node const & node : graph.nodes) {
      os << "\nNode (" << &node << "; BB " << node.block << ")";
      os << "\nFirst instruction:";
      os << "\n\t" << *node.block->begin();
      os << "\nEdges:";
      for (Edge const * edge : graph.tree_edges.at(&node)) {
        auto other_result = edge->other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        os
          << "\n\tEdge (" << edge << ")"
          << ": " << edge->first << " → " << edge->second
          << "\n\tclass: " << edge->cycle_class;
      }
      if (print_backedges) {
        os << "\nBackedges:";
        for (Edge const * backedge : graph.backedge_graph.at(&node)) {
          auto other_result = backedge->other_end(&node);
          if (!other_result.first)
            assert(0 && "Backedge did not have other end?");
          Node const * other = other_result.second;
          int print_class = backedge->cycle_class;
          os
            << "\n\tBackedge (" << backedge << ") → " << other
            << "\n\tclass: " << print_class;
        }
        os << "\nCapping Backedges:";
        for (Edge const & backedge : graph.capping_backedges.at(&node)) {
          auto other_result = backedge.other_end(&node);
          if (!other_result.first)
            assert(0 && "Capping backedge did not have other end?");
          Node const * other = other_result.second;
          int print_class = backedge.cycle_class;
          os
            << "\n\tCapping Backedge (" << &backedge << ") → " << other
            << "\n\tclass: " << print_class;
        }
      }
    }
  }

  void Graph::print_brackets (
    Graph const & graph,
    llvm::raw_ostream & os
  ) {
    assert(graph.valid);
    os << "Graph BracketLists:";
    for (Node const & node : graph.nodes) {
      os
        << "\nNode (" << &node << "; BB " << node.block << ")"
        << "\nFirst instruction:"
        << "\n\t" << *node.block->begin();
      for (Edge const * bracket : node.bracket_list.brackets) {
        int print_class = bracket->cycle_class;
        os
          << "\n\tBracket (Edge): " << bracket
          << " class: " << print_class;
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
  using Annotation = TalkDown::Annotation;
  Annotation parse_metadata (MDNode * md) {
    // NOTE(jordan): MDNode is a tuple of MDString, MDString pairs
    // NOTE(jordan): Use mdconst::dyn_extract API from Metadata.h#483
    Annotation result = {};
    for (auto const & pair_operand : md->operands()) {
      using namespace llvm;
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

namespace SESE {
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
    enum class StructureType { Canonical, NonCanonical, Indeterminate };
    int depth;
    EnclosesType enclosesType;
    StructureType structureType;
    Region const * parent;
    std::vector<Region const *> children;
    // NOTE(jordan): only a Region of EnclosesType::Block has a block.
    BasicBlock const * block = nullptr;
  };
  struct Tree {
    int height;
    Region const * root;
    std::vector<Region> regions;
    std::vector<Region const *> canonical_regions;
    std::vector<Region const *> noncanonical_regions;
    std::vector<Region const *> bb_regions;
    std::map<Region const *, Note::Annotation> annotations;
    static Tree compute (CycleEquivalence::Graph const &);
    // NOTE(jordan): traversal methods.
    static Region const * innermostRegionForBlock (BasicBlock const *);
    static Region const * out (Region const &);
    static Region const * in  (Region const &);
    static Region const * out_canonical (Region const &);
    static Region const * in_canonical  (Region const &);
    // NOTE(jordan): annotation getter/setters.
    using string = std::string;
    static bool hasAnnotation (string, Region const *);
    static std::string getAnnotation (string, Region const *);
    static void upsertAnnotation (string, string, Region const *);
  };

  Region const * Tree::innermostRegionForBlock (BasicBlock const *) {
    // TODO
  }
  Region const * Tree::out (Region const &) {
    // TODO(jordan): check and return parent.
  }
  Region const * Tree::in  (Region const &) {
    // TODO(jordan): choose an arbitrary (first) sibling and return it.
  }
  Region const * Tree::out_canonical (Region const &) {
    // TODO(jordan): return nearest Canonical parent.
  }
  Region const * Tree::in_canonical  (Region const &) {
    // TODO(jordan): recursively find 1st Canonical descendant.
  }

  Tree compute (CycleEquivalence::Graph const & graph) {
    // TODO
  }
}

bool llvm::TalkDown::doInitialization (Module &M) {
  return false;
}

// TODO(jordan): it looks like this can be refactored as a FunctionPass
bool llvm::TalkDown::runOnModule (Module &M) {
  /* 1. Identify annotation ranges
   * 2. Split BasicBlocks wherever the annotation changes
   * 3. Construct SESE Tree at BasicBlock granularity; write query APIs
   * 4. Add SESE Tree regions for annotation spans (excepting DAG cases)
   * 5. ... Handle DAG cases? Annotation spans do not necessarily map to
   *    SESE Regions at all. Annotations are structurally, not nominally,
   *    unique. The same annotation can be / is shared by many Regions,
   *    even outside of SESE Tree boundaries. e.g. an Annotation can/will
   *    span Regions across nesting levels by a stack discipline. We
   *    cannot reproduce these spans from the metadata emitted from the
   *    front-end. TODO: figure out what to actually do here? We are only
   *    able to (re)construct certain types of annotation spans. That's
   *    annoying.
   */

  using SplitPoint = Instruction *;
  std::vector<SplitPoint> splits = {};

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
  /* for (SplitPoint & split : splits) { */
  /*   llvm::errs() */
  /*     << "Split:" */
  /*     << "\n\tin block @ " << split->getParent() */
  /*     << "\n\tbefore instruction @ " << split */
  /*     << "\n\t" << *split */
  /*     << "\n"; */

  /*   // NOTE(jordan): DEBUG */
  /*   /1* BasicBlock::iterator I (split); *1/ */
  /*   /1* Instruction * previous = &*--I; *1/ */
  /*   /1* if (previous->hasMetadata()) { *1/ */
  /*   /1*   MDNode * noelle_meta = previous->getMetadata("note.noelle"); *1/ */
  /*   /1*   llvm::errs() << previous << " has Noelle annotation:\n"; *1/ */
  /*   /1*   Annotation note = Note::parse_metadata(noelle_meta); *1/ */
  /*   /1*   Note::print_annotation(note, llvm::errs()); *1/ */
  /*   /1* } *1/ */

  /*   // NOTE(jordan): using SplitBlock is recommended in the docs */
  /*   llvm::SplitBlock(split->getParent(), split); */
  /* } */
  /* llvm::errs() << "Splits made.\n"; */

  // Construct SESE tree
  llvm::errs() << "\n";
  for (auto & function : M) {
    using namespace SESE;
    auto undirected_cfg = UndirectedCFG::compute(function);
    llvm::errs() << "Undirected CFG for " << function.getName() << "\n";
    if (!undirected_cfg.valid) {
      llvm::errs() << "(graph is invalid)";
    } else if (undirected_cfg.empty) {
      llvm::errs() << "(graph is empty)";
    } else {
      UndirectedCFG::print(undirected_cfg, llvm::errs());
    }
    llvm::errs() << "\n\n";
    llvm::errs() << "Spanning Tree for " << function.getName() << "\n";
    auto spanning_tree = SpanningTree::compute(undirected_cfg);
    if (!spanning_tree.valid) {
      llvm::errs() << "(spanning tree is invalid)";
    } else if (spanning_tree.empty) {
      llvm::errs() << "(spanning tree is empty)";
    } else {
      SpanningTree::print(spanning_tree, llvm::errs());
    }
    llvm::errs() << "\n\n";
    llvm::errs() << "Cycle Equiv. for " << function.getName() << "\n";
    auto graph = CycleEquivalence::Graph::compute(spanning_tree);
    if (!graph.valid) {
      llvm::errs() << "(cycle equivalence graph is invalid)";
    } else if (graph.empty) {
      llvm::errs() << "(cycle equivalence graph is empty)";
    } else {
      CycleEquivalence::Graph::print(graph, llvm::errs(), true);
      /* llvm::errs() << "\n"; */
      /* CycleEquivalence::Graph::print_brackets(graph, llvm::errs()); */
    }
    llvm::errs() << "\n\n";
    /* TODO(jordan): using the cycle equivalence graph, compute an SESE
     * tree for the function.
     *
     * Visit every node in the CycleEquivalence::Graph, preferring Edges
     * of the highest cycle_class during traversal. This way we will start
     * at the entry node and end at the exit node. We can group the basic
     * blocks into SESE Regions, and nest the Regions into a Tree.
     *
     * 1. Construct bb_regions (Block-enclosing).
     * 2. Construct canonical_regions (Region-enclosing).
     * 3. Split/merge into annotation_regions (Region-enclosing).
     */
    // 1. Ingest every basic block into a Block-enclosing Region
    std::vector<SESE::Region> bb_regions;
    for (auto const & block : function) {
      bb_regions.push_back({
        /* depth         */ -1,
        /* enclosesType  */ SESE::Region::EnclosesType::Block,
        /* structureType */ SESE::Region::StructureType::Indeterminate,
        /* parent        */ nullptr,
        /* children      */ {},
        /* block         */ &block,
      });
    }
    // 2. Traverse CycleEquivalence::Graph to construct Canonical Regions
    std::vector<SESE::Region> canonical_regions = {
      /* Top Region */
      {
        /* depth         */ 0,
        /* enclosesType  */
          function.size() == 1
          ? SESE::Region::EnclosesType::Block
          : SESE::Region::EnclosesType::Region,
        /* structureType */ SESE::Region::StructureType::Canonical,
        /* parent        */ nullptr,
      }
    };
    SESE::Region const & current = canonical_regions.back();
    // TODO(jordan): like... all of it.
  }

  return true; // blocks are split; source is modified
}

void llvm::TalkDown::getAnalysisUsage (AnalysisUsage &AU) const {
  /* NOTE(jordan): I'm pretty sure this analysis is non-preserving of
   * other analyses. Control flow changes, for example, when basic blocks
   * are split. It would be difficult to not do this, but possible.
   */
  /* AU.setPreservesAll(); */
  // NOTE(jordan): ensures a single exit node for each IR function.
  AU.addRequired<UnifyFunctionExitNodes>();
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
