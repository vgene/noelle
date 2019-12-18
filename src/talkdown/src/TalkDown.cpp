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
// Abstract Edge type
#if 0
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
#endif
}

namespace SESE {
namespace UndirectedCFG {
#if 0
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
#endif

  Graph compute (llvm::Function & function) {
    if (std::begin(function) == std::end(function)) {
      return { /* valid */ true, /* empty */ true };
    }
    std::vector<Node> nodes;
    uint64_t counter = 0;
    for (llvm::BasicBlock & block : function) {
      // NOTE(jordan): do not handle unreachable blocks.
      if (true
        && &block != &function.getEntryBlock()
        && llvm::pred_begin(&block) == llvm::pred_end(&block)
      ) {
        llvm::errs() << "Unreachable block found! Aborting...\n";
        return { /* valid */ false };
      }
      nodes.push_back({ counter, &block });
      counter++;
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
    for (auto const &node : graph.nodes)
    {
      os << "********************************************************************************\n";
      os << "Node " << node.number << "; block " << node.block << "\n";
      node.block->dump();
      os << "--------------------------------------------------------------------------------\n";
    }
    for (auto const & entry : graph.edges) {
      auto const & a = entry.first;
      auto const & b = entry.second;
      os
        << "\nEdge:"
        << "\n\tNode (" << a->number << "; BB " << a->block << ")"
        << "\n\tNode (" << b->number << "; BB " << b->block << ")";
    }
  }
} // namespace UndirectedCFG
} // namespace SESE

namespace SESE {
namespace SpanningTree {
#if 0
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
#endif

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
    for (auto & edge : graph.edges) {
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
        node->cfg_backedges.push_back(&edge);
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
    for (Node * node : nodes) {
      for (UndirectedCFG::Edge const * bb_backedge : node->cfg_backedges) {
        BasicBlock const * seek_bb = nullptr;
        if (bb_backedge->first->block == node->block) {
          seek_bb = bb_backedge->second->block;
        } else {
          seek_bb = bb_backedge->first->block;
        }
        Node * reached_node = nullptr;
        for (auto & seek_node : nodes) {
          if (seek_node->block == seek_bb) {
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
        if (bb_backedge->first->block == node->block) {
          backedges.push_back({ node, reached_node });
        } else {
          backedges.push_back({ reached_node, node });
        }
        node->backedges.push_back(reached_node);
        reached_node->backedges.push_back(node);
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
#if 0
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
#endif

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
   * presence of an unstructured loop. It's not immediately obvious how to
   * detect this, however.
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
    // 2. Construct edge pointer maps for children, back-edges.
    std::map<Node const *, std::vector<Edge *>> tree_map;
    std::map<Node const *, std::vector<Edge *>> backedge_map;
    std::vector<Edge> all_backedges;
    std::vector<Edge> all_edges;
    // 2.a. Compute sets of edges and backedges & initialize maps.
    for (auto & node : dfs_nodes) {
      for (auto & child_index : node.children) {
        Node const & child = dfs_nodes.at(child_index);
        all_edges.push_back({ &node, &child });
      }
      tree_map.insert({ &node, {} });
      backedge_map.insert({ &node, {} });
    }
    for (auto & backedge : tree.backedges) {
      Node const & node = dfs_nodes.at(backedge.first->dfs_index);
      Node const & dest = dfs_nodes.at(backedge.second->dfs_index);
      all_backedges.push_back({ &node, &dest });
    }
    // gc14: is this part of the algorithm?
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
    /* XXX gc14: commented this out to test for now */
    all_backedges.push_back({ &exit, &start });
    exit.backedges.push_back(start.dfs_index);
    if (&start != &exit) start.backedges.push_back(exit.dfs_index);
    /**/
    // 2.c. Compute the graph of forward-edges.
    for (auto & edge : all_edges) {
      Node const & node = *edge.first;
      Node const & dest = *edge.second;
      tree_map.at(&node).push_back(&edge);
      tree_map.at(&dest).push_back(&edge);
    }
    // 2.d. Compute the graph of back-edges.
    for (auto & backedge : all_backedges) {
      Node const & node = *backedge.first;
      Node const & dest = *backedge.second;
      backedge_map.at(&node).push_back(&backedge);
      backedge_map.at(&dest).push_back(&backedge);
    }
    // 3.0. Allocate a capping backedges map.
    std::map<Node const *, std::vector<std::unique_ptr<Edge>>>
    capping_backedges = {};
    for (Node const & node : dfs_nodes) {
      // NOTE(jordan): preallocate a capping_backedges vector.
      // NOTE(jordan): move semantics. "semantics."
      std::vector<std::unique_ptr<Edge>> fuck_this = {};
      capping_backedges.emplace(&node, std::move(fuck_this));
    }
    // 3.1. Calculate cycle equivalence.
    // The Program Structure Tree. Section 3.5. Figure 4. p178
    // "for each node in reverse depth-first order"
    for (auto it = dfs_nodes.rbegin(); it != dfs_nodes.rend(); it++) {
      Node & node = *it;

      // "compute n.hi"
      // hi0 = min({ t.dfsnum | (n, t) is a back-edge })
      /* // NOTE(jordan): DEBUG */
      /* llvm::errs() << "backedges"; */
      /* for (int backedge_dfs_index : node.backedges) { */
      /*   Node const & other = dfs_nodes.at(backedge_dfs_index); */
      /*   llvm::errs() */
      /*     << "\n\t" << &other */
      /*     << "\n\tdfsnum " << backedge_dfs_index */
      /*     << "\n\thi " << other.hi */
      /*     << "\n\tfirst instruction " << *other.block->begin(); */
      /* } */
      /* llvm::errs() << "\n"; */
      auto hi0_dfsnum_it = std::min_element(
        std::begin(node.backedges),
        std::end(node.backedges)
      );
      int hi0 = INT_MAX;
      if (hi0_dfsnum_it != std::end(node.backedges)) {
        hi0 = *hi0_dfsnum_it;
      }
      // hi1 = min({ c.hi | c is a child of n })
      /* // NOTE(jordan): DEBUG */
      /* llvm::errs() << "children"; */
      /* for (int child_dfs_index : node.children) { */
      /*   Node const & child = dfs_nodes.at(child_dfs_index); */
      /*   llvm::errs() */
      /*     << "\n\t" << &child */
      /*     << "\n\tdfsnum " << child_dfs_index */
      /*     << "\n\thi " << child.hi */
      /*     << "\n\tfirst instruction " << *child.block->begin(); */
      /* } */
      /* llvm::errs() << "\n"; */
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
      /* // NOTE(jordan): DEBUG */
      /* llvm::errs() << "lo_children"; */
      /* for (int lo_child_index : lo_children) { */
      /*   Node const & lo_child = dfs_nodes.at(lo_child_index); */
      /*   llvm::errs() */
      /*     << "\n\t" << &lo_child */
      /*     << "\n\thi " << lo_child.hi */
      /*     << "\n\tfirst instruction " << *lo_child.block->begin(); */
      /* } */
      /* llvm::errs() << "\n"; */
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
      /* // NOTE(jordan): DEBUG */
      /* llvm::errs() */
      /*     << "hi2 " << hi2 */
      /*     << " hi1 " << hi1 */
      /*     << " hi0 " << hi0 */
      /*     << "\n"; */
      // "compute bracketlist"
      // for each child c of n do
      for (int const child_dfs_index : node.children) {
        // n.blist = concat(c.blist, n.blist)
        Node const & child = dfs_nodes.at(child_dfs_index);
        node.bracket_list.concat(child.bracket_list);
      }
      llvm::errs() << "Bracket list of node " << &node << ":\n";
      for (int i = 0; i < node.bracket_list.size(); i++)
        llvm::errs() << "\t" << node.bracket_list.brackets[i]->first << " <--> " << node.bracket_list.brackets[i]->second << "\n";
      // for each capping backedge d from a descendant of n to n do
      for (auto const & up_backedge : capping_backedges.at(&node)) {
        Edge const & capping_backedge = *up_backedge;
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
      for (Edge * backedge : backedge_map.at(&node)) {
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
              << "Assigning Edge (" << backedge << ")"
              << " class as Backedge: " << cycle_classes
              << "\n";
            backedge->cycle_class = cycle_classes++;
          }
        }
      }
      // for each backedge e from n to an ancestor of n do
      for (Edge * backedge : backedge_map.at(&node)) {
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
      if (hi2 < hi0) {
        Node const & hi2_node = dfs_nodes.at(hi2);
        /* NOTE(jordan): adjustment to the algorithm:
         * A capping backedge from n to n is trivially empty: it cannot be
         * the bracket for any acestor edges, because it does not enclode
         * any ancestor edges. So, do not create a capping backedge if it
         * would immediately be removed.
         */
        if (&hi2_node != &node) {
          // "create capping backedge"
          llvm::errs()
            << "\"create capping backedge\""
            << "\nNode (" << &node << " ; BB " << node.block << ")"
            << "\nfirst instruction: " << *node.block->begin()
            << "\n";
          // d = (n, node[hi2])
          std::unique_ptr<Edge> up_backedge(new Edge(&node, &hi2_node));
          // push(n.blist, d)
          node.bracket_list.push(up_backedge.get());
          /* NOTE(jordan): add the capping backedge to the ancestor so
           * that it will be found and removed from its bracket-list. This
           * is more clever than obvious, since the capping backedge does
           * not technically belong to either node; but it can only be
           * held by one, since it is a unique_ptr. We could perhaps
           * change to shared_ptr, but it would not make the code
           * meaningfully more obvious and it would be less efficient.
           * More obvious would be to have the same pattern for capping
           * backedges as regular backedges. Meh. TODO?
           */
          capping_backedges.at(&hi2_node).push_back(std::move(up_backedge));
        }
      }

      // "determine class for edge from parent(n) to n
      // if n is not the root of dfs tree:
      // NOTE(jordan): the root has a parent dfs_index of -1
      if (node.parent != -1) {
        // let e be the tree edge from parent(n) to n
        auto const & parent = dfs_nodes.at(node.parent);
        auto & parent_edges = tree_map.at(&parent);
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
        if ( node.bracket_list.size() == 0 )
        {
          llvm::errs() << "Bracket list for node " << node.block << " of size 0!\n";
          /* return {false}; */
        }
        if ( node.bracket_list.top() == nullptr )
        {
          llvm::errs() << "Top of bracket list for node " << node.block << " is nullptr!\n";
          /* return {false}; */
        }
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
      /* cycle_classes      */ std::move(cycle_classes),
      /* start              */ std::move(&start),
      /* exit               */ std::move(&exit),
      /* nodes              */ std::move(dfs_nodes),
      /* edges              */ std::move(all_edges),
      /* backedges          */ std::move(all_backedges),
      /* tree_map           */ std::move(tree_map),
      /* backedge_map       */ std::move(backedge_map),
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
      for (Edge const * edge : graph.tree_map.at(&node)) {
        auto other_result = edge->other_end(&node);
        if (!other_result.first)
          assert(0 && "Edge did not have other end?");
        Node const * other = other_result.second;
        os
          << "\n\tEdge (" << edge << ")"
          << " " << edge->first << " → " << edge->second
          << "\n\tclass: " << edge->cycle_class;
      }
      if (print_backedges) {
        os << "\nBackedges:";
        for (Edge const * backedge : graph.backedge_map.at(&node)) {
          int print_class = backedge->cycle_class;
          os
            << "\n\tBackedge (" << backedge << ")"
            << " " << backedge->first << " → " << backedge->second
            << "\n\tclass: " << print_class;
        }
        os << "\nCapping Backedges:";
        for (auto const & up_cap : graph.capping_backedges.at(&node)) {
          Edge const & backedge = *up_cap;
          // NOTE(jordan): a capping backedge is fake and has no class
          os
            << "\n\tCapping Backedge (" << &backedge << ")"
            << " " << backedge.first << " → " << backedge.second;
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

  void sorting_append (
    std::vector<Edge> const & source,
    std::vector<Edge const *> & destination
  ) {
    for (Edge const & next_edge : source) {
      /* NOTE(jordan): find the iterator to the first edge with a lower
       * cycle class and then insert this edge before it.
       */
      auto insertion_point_it = std::find_if(
        std::begin(destination),
        std::end(destination),
        [&] (Edge const * edge) {
          return next_edge.cycle_class > edge->cycle_class;
        }
      );
      destination.insert(insertion_point_it, &next_edge);
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

#if 0
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
#endif

namespace SESE {
#if 0
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
#endif

  std::vector<Region *> getAllOffspringRegionsRecursive( Region *reg )
  {
    std::vector<Region *> offspring = {};

    if ( reg->enclosesType == SESE::Region::EnclosesType::Block )
      return { reg };

    for ( Region *reg : reg->children )
    {
      if ( reg->enclosesType != SESE::Region::EnclosesType::Block )
        offspring.push_back(reg);
      std::vector<Region *> ret = getAllOffspringRegionsRecursive(reg);
      offspring.insert(offspring.end(), ret.begin(), ret.end());
    }

    return offspring;
  }

  void Region::getAllOffspringRegions( void )
  {
    region_offspring = getAllOffspringRegionsRecursive(this);
  }

  Region *Tree::getFirstCommonAncestor(Region *r1, Region *r2)
  {
    assert(r1->depth >= 0);
    assert(r2->depth >= 0);

    // if one region has a deeper depth, go up in tree until same depth
    Region *same_depth, *other;
    if ( r1->depth > r2->depth )
    {
      same_depth = r1;
      other = r2;
    }
    else if ( r1->depth < r2->depth )
    {
      same_depth = r2;
      other = r1;
    }
    while ( same_depth != nullptr && same_depth->depth > other->depth )
      same_depth = same_depth->parent;

    while ( same_depth != root )
    {
      if ( same_depth == other )
        break;
      same_depth = same_depth->parent;
      other = other->parent;
    }

    if ( same_depth == root )
      same_depth = nullptr;

    return same_depth;
  }

  bool Tree::validAnnotation(llvm::Instruction *i1, llvm::Instruction *i2, std::string a)
  {
    assert(i1 != nullptr);
    assert(i2 != nullptr);

    llvm::BasicBlock *bb1 = i1->getParent();
    llvm::BasicBlock *bb2 = i2->getParent();
    Region *reg1 = innermostRegionForBlock(bb1);
    Region *reg2 = innermostRegionForBlock(bb2);

    Region *ancestor = getFirstCommonAncestor(reg1, reg2);

    for ( auto *child : ancestor->children )
    {
      if ( !hasAnnotation(a, child) )
        return false;
    }

    return true;
  }

  std::vector<Region *> Tree::getAllLeafOffspring(Region *r)
  {
    std::vector<Region *> leaves;

    // in leaf
    if ( r->enclosesType == SESE::Region::EnclosesType::Block )
      return { r };

    // get leaves of all children
    for ( auto *child : r->children )
    {
      std::vector<Region *> ret = getAllLeafOffspring(child);
      leaves.insert(leaves.end(), ret.begin(), ret.end());
    }
  }

  // NOTE(jordan): because there are regions for every block, this works.
  Region * Tree::innermostRegionForBlock (BasicBlock const * block) {
    for (auto & region : regions) {
      if (region.enclosesType == SESE::Region::EnclosesType::Block) {
        if (region.block == block) {
          return &region;
        }
      }
    }
    return nullptr;
  }
  Region * Tree::out (Region const & current) {
    assert(current.parent != nullptr);
    return current.parent;
  }
  Region * Tree::in  (Region const & current) {
    if (current.children.size() == 0) {
      return nullptr;
    }
    return current.children.at(0);
  }
  Region * Tree::out_canonical (Region const & current) {
    assert(current.parent != nullptr);
    Region * outer = current.parent;
    while (outer->structureType != SESE::Region::StructureType::Canonical) {
      assert(outer->parent != nullptr);
      outer = outer->parent;
    }
    return outer;
  }
  Region * Tree::in_canonical  (Region const & current) {
    if (current.children.size() == 0) {
      return nullptr;
    }
    for (Region * child : current.children) {
      if (child->structureType == SESE::Region::StructureType::Canonical) {
        return child;
      } else {
        return in_canonical(*child);
      }
    }
  }

  void find_region_boundaries_recursive (
    Node const & node,
    std::map<Edge const *, bool> & visited_edges,
    std::map<Node const *, bool> & visited_nodes,
    std::vector<Edge const *> const & sorted_edges,
    std::vector<Edge const *> region_starts,
    std::vector<RegionBoundary> & boundaries
  ) {
    // Find the unvisted edges at this node in cycle_class order
    std::vector<Edge const *> next_edges = {};
    for (Edge const * candidate_ptr : sorted_edges) {
      Edge const & candidate = *candidate_ptr;
      if (candidate.first == &node) {
        next_edges.push_back(&candidate);
      }
    }
    // Traverse all of the unvisited next edges in cycle_class order
    for (Edge const * next_edge_ptr : next_edges) {
      Edge const & next_edge = *next_edge_ptr;
      if (visited_edges.at(&next_edge)) {
        continue;
      }
      // Mark the edge visited
      visited_edges.at(&next_edge) = true;
      // Check if the edge closes a region
      if (region_starts.at(next_edge.cycle_class) != nullptr) {
        Edge const * start_ptr = region_starts.at(next_edge.cycle_class);
        boundaries.push_back({ start_ptr, &next_edge });
      }
      // Find the other end of the Edge
      auto other_end_result = next_edge.other_end(&node);
      assert(other_end_result.first && "Edge does not have other end?");
      Node const * other_end = other_end_result.second;
      // If the other end has been visited, skip it
      if (visited_nodes.at(other_end)) {
        continue;
      }
      // If the other end has NOT been visited:
      // 1. Mark it visited
      visited_nodes.at(other_end) = true;
      // 2. Start a new region from this edge
      auto region_starts_copy = region_starts;
      region_starts_copy.at(next_edge.cycle_class) = &next_edge;
      // 3. Recur
      find_region_boundaries_recursive(
        *other_end,
        visited_edges,
        visited_nodes,
        sorted_edges,
        std::move(region_starts_copy),
        boundaries
      );
    }
  }

  /* NOTE(jordan): find_region_boundaries performs a
   * highest-cycle_class-priority, depth-first traversal of the graph,
   * gathering
   */
  std::vector<RegionBoundary> find_region_boundaries (
    CycleEquivalence::Graph const & graph
  ) {
    if (graph.empty) return {};
    Node const & start = *graph.start;
    std::vector<Edge const *> edges = {};
    CycleEquivalence::sorting_append(graph.edges, edges);
    CycleEquivalence::sorting_append(graph.backedges, edges);
    // Initialize visited_edges to all false
    std::map<Edge const *, bool> visited_edges = {};
    for (Edge const * edge : edges) {
      visited_edges.insert({ edge, false });
    }
    // Initialize visited_nodes to all false
    std::map<Node const *, bool> visited_nodes = {};
    for (Node const & node : graph.nodes) {
      visited_nodes.insert({ &node, false });
    }
    visited_nodes.at(&start) = true;
    // Initialize region_Starts to all nullptr
    std::vector<Edge const *> region_starts (graph.cycle_classes);
    for (int cc = 0; cc < graph.cycle_classes; cc++) {
      region_starts.at(cc) = nullptr;
    }
    std::vector<RegionBoundary> boundaries = {};
    find_region_boundaries_recursive(
      start,
      visited_edges,
      visited_nodes,
      edges,
      region_starts,
      boundaries
    );
    return std::move(boundaries);
  }

  void reify_regions_recursive (
    Node const & node,
    Region * parent_region,
    std::map<Edge const *, bool> & visited_edges,
    std::map<Node const *, bool> & visited_nodes,
    std::vector<Edge const *> const & edges,
    std::vector<RegionBoundary> & boundaries,
    std::vector<Region> & regions
  ) {
    std::vector<Edge const *> node_edges = {};
    for (Edge const * candidate_ptr : edges) {
      Edge const & candidate = *candidate_ptr;
      if (candidate.touches(&node)) {
        node_edges.push_back(&candidate);
      }
    }
    for (Edge const * next_edge_ptr : node_edges) {
      Edge const & next_edge = *next_edge_ptr;
      // Skip in-edges and visited edges (we'll get to the in-edges later)
      if (next_edge.first != &node) {
        continue;
      }
      if (visited_edges.at(&next_edge)) {
        continue;
      }
      Region * edge_parent = parent_region;
      // Mark the edge visited
      llvm::errs() << "visiting edge " << &next_edge << "\n";
      visited_edges.at(&next_edge) = true;
      // Check if the edge closes a region
      auto closed_it = std::find_if(
        std::begin(boundaries),
        std::end(boundaries),
        [&] (RegionBoundary const & boundary) {
          return boundary.end == &next_edge;
        }
      );
      // If the edge closes a region, move up a parent
      if (closed_it != std::end(boundaries)) {
        assert(parent_region->parent != nullptr);
        edge_parent = parent_region->parent;
        RegionBoundary & closed = *closed_it;
        /* If the closed boundary edges are the only edges, change the
         * parent (the region we are closing) to a block-enclosing region.
         */
        bool was_block_region = true
          && (node_edges.size() == 2)
          && (closed.start->second == closed.end->first)
          ;
        if (was_block_region) {
          parent_region->enclosesType = SESE::Region::EnclosesType::Block;
          parent_region->block = node.block;
        }
        llvm::errs()
          << "Closing" << (was_block_region ? " BLOCK " : " ")
          << "region (" << parent_region << ")"
          << " depth " << parent_region->depth
          << "\n  returning to (" << edge_parent << ")"
          << " depth " << edge_parent->depth
          << "\n";
      }
      // Find the other end of the Edge
      auto other_end_result = next_edge.other_end(&node);
      assert(other_end_result.first && "Edge does not have other end?");
      Node const * other_end = other_end_result.second;
      // If the other end has been visited, skip it
      if (visited_nodes.at(other_end)) {
        continue;
      }
      // If the other end has NOT been visited:
      // Check if the edge opens a region
      auto opened_it = std::find_if(
        std::begin(boundaries),
        std::end(boundaries),
        [&] (RegionBoundary const & boundary) {
          return boundary.start == &next_edge;
        }
      );
      // If the edge opens a region, construct it & set it to parent
      if (opened_it != std::end(boundaries)) {
        RegionBoundary & boundary = *opened_it;
        regions.push_back({
          /* depth         */ edge_parent->depth + 1,
          /* enclosesType  */ SESE::Region::EnclosesType::Region,
          /* structureType */ SESE::Region::StructureType::Canonical,
          /* parent        */ edge_parent,
          /* start         */ boundary.start,
          /* end           */ boundary.end,
          /* children      */ {},
        });
        Region & opened_region = regions.back();
        llvm::errs()
          << "Opening region (" << &opened_region << ")"
          << " depth " << opened_region.depth
          << "\n  from parent (" << edge_parent << ")"
          << " depth " << edge_parent->depth
          << "\n";
        edge_parent->children.push_back(&opened_region);
        edge_parent = &opened_region;
      }
      visited_nodes.at(other_end) = true;
      reify_regions_recursive(
        *other_end,
        edge_parent,
        visited_edges,
        visited_nodes,
        edges,
        boundaries,
        regions
      );
    }
    /* Create a non-canonical block-region for this node if we have not
     * created a region for it yet.
     */
    if (!(parent_region->enclosesType == SESE::Region::EnclosesType::Block)) {
      regions.push_back({
        /* depth         */ parent_region->depth + 1,
        /* enclosesType  */ SESE::Region::EnclosesType::Block,
        /* structureType */ SESE::Region::StructureType::NonCanonical,
        /* parent        */ parent_region,
        /* start         */ nullptr,
        /* end           */ nullptr,
        /* children      */ {},
        /* block         */ node.block,
      });
    }
  }

  std::pair<SESE::Region *, std::vector<Region>> reify_regions (
    std::vector<RegionBoundary> boundaries,
    CycleEquivalence::Graph const & graph
  ) {
    if (graph.empty) return {};
    Node const & start = *graph.start;
    std::vector<Edge const *> edges = {};
    CycleEquivalence::sorting_append(graph.edges, edges);
    CycleEquivalence::sorting_append(graph.backedges, edges);
    std::map<Edge const *, bool> visited_edges = {};
    for (Edge const * edge : edges) {
      visited_edges.insert({ edge, false });
    }
    std::map<Node const *, bool> visited_nodes = {};
    for (Node const & node : graph.nodes) {
      visited_nodes.insert({ &node, false });
    }
    visited_nodes.at(&start) = true;
    std::vector<Region> regions = {};
    regions.reserve(boundaries.size() + graph.nodes.size() + 1);
    regions.push_back({
      /* depth         */ 0,
      /* enclosesType  */ SESE::Region::EnclosesType::Region,
      /* structureType */ SESE::Region::StructureType::Canonical,
      /* parent        */ nullptr,
      /* start         */ nullptr,
      /* end           */ nullptr,
      /* children      */ {},
    });
    SESE::Region & root_region = regions.back();
    reify_regions_recursive(
      start,
      &root_region,
      visited_edges,
      visited_nodes,
      edges,
      boundaries,
      regions
    );
    return {
      &root_region,
      std::move(regions),
    };
  }

  Tree Tree::compute (CycleEquivalence::Graph const & graph) {
    if (!graph.valid) return { /* valid */ false };
    if (graph.empty)  return { /* valid */ true, /* empty */ true };
    // Find boundaries and then construct regions
    auto region_boundaries = SESE::find_region_boundaries(graph);
    auto reify_result = SESE::reify_regions(region_boundaries, graph);
    Region const * root = reify_result.first;

    /* After we have all the regions, populate the basic_block_offspring
     * of each region */

    std::vector<Region> & regions = reify_result.second;
    return {
      /* valid   */ true,
      /* empty   */ false,
      /* root    */ root,
      /* regions */ std::move(regions),
    };
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
   */

  using SplitPoint = Instruction *;
  std::vector<SplitPoint> splits = {};

  // Collect all the split points in each function
  // TODO(jordan): refactor this into its own function.
  for (auto & function : M) {
    MDNode * last_note_meta = nullptr;
    // if function declaration, skip creating a SESE tree for it
    if ( function.isDeclaration() )
      continue;
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
            Note::Annotation note = Note::parse_metadata(last_note_meta);
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

    auto spanning_tree = SpanningTree::compute(undirected_cfg);
    llvm::errs() << "Spanning Tree for " << function.getName() << "\n";
    if (!spanning_tree.valid) {
      llvm::errs() << "(spanning tree is invalid)";
    } else if (spanning_tree.empty) {
      llvm::errs() << "(spanning tree is empty)";
    } else {
      SpanningTree::print(spanning_tree, llvm::errs());
    }
    llvm::errs() << "\n\n";
    llvm::errs() << "Done with printing spanning tree\n";

    auto graph = CycleEquivalence::Graph::compute(spanning_tree);
    llvm::errs() << "Cycle Equiv. for " << function.getName() << "\n";
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

    SESE::Tree sese_tree = SESE::Tree::compute(graph);
    /* this->sese_tree = SESE::Tree::compute(graph); */
    llvm::errs().changeColor(llvm::raw_ostream::GREEN);
    llvm::errs() << "SESE Tree for " << function.getName() << "\n";
    llvm::errs().resetColor();
    SESE::Region const * root_ptr = sese_tree.root;
    std::vector<Region> & regions = sese_tree.regions;
    if (!sese_tree.valid) {
      llvm::errs() << "(sese tree is invalid)";
    } else if (sese_tree.empty) {
      llvm::errs() << "(sese tree is empty)";
    } else {
      for (auto & block : function) {
        auto & first_inst = block.front();
        MDNode * note_meta = first_inst.getMetadata("note.noelle");
        Region * region = sese_tree.innermostRegionForBlock(&block);
        Note::Annotation note = {};
        if (note_meta != nullptr) {
          note = Note::parse_metadata(note_meta);
        }
        assert(region != nullptr);
        region->annotation = note;
        sese_tree.annotations.insert({ region, note });
      }
      int num_regions = 0;
      for (auto & region : regions) {
        bool canonical = (region.structureType == SESE::Region::StructureType::Canonical);
        bool block_size = (region.enclosesType == SESE::Region::EnclosesType::Block);
        region.getAllOffspringRegions();
        llvm::errs().changeColor(llvm::raw_ostream::GREEN);
        llvm::errs()
          << "\nRegion (" << &region << ")"
          << (canonical  ? " CANONICAL" : "")
          << (block_size ? " BLOCK"     : "");
        llvm::errs().resetColor();
        llvm::errs()
          << "\n  depth  " << region.depth
          << "\n  parent " << region.parent
          << "\n  start  " << region.start
          << "\n  end    " << region.end
          << "\n  block  " << region.block << (region.block != nullptr ? " ==> " + region.block->getName() : "");
        llvm::errs() << "\n";
        Note::Annotation const & note = region.annotation;
        Note::print_annotation(note, llvm::errs());
        llvm::errs().changeColor(llvm::raw_ostream::YELLOW);
        llvm::errs() << "\nOffspring regions {\n";
        llvm::errs().resetColor();
        for ( auto *offspring : region.region_offspring )
        {
          llvm::errs() << "  " << offspring << "\n";
        }
        llvm::errs().changeColor(llvm::raw_ostream::YELLOW);
        llvm::errs() << "}";
        llvm::errs().resetColor();
        num_regions++;
      }
      llvm::errs().changeColor(llvm::raw_ostream::BLUE);
      llvm::errs() << "\nNumber of regions: ";
      llvm::errs().resetColor();
      llvm::errs() << num_regions;
    }
    // TODO(jordan): coalescing
    // Sort regions by depth in ascending order (deepest-first)
    // Propagate annotations to parents that are shared by all children
    llvm::errs() << "\n";
  }

  return true; // blocks are split; source is modified
}

void llvm::TalkDown::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.addRequired<UnifyFunctionExitNodes>();
  return;
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
