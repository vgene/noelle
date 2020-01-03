#pragma once

#include "llvm/IR/Instructions.h"

#include <vector>
#include <unordered_map>

using namespace llvm;

namespace llvm {

  /*
   * Each node is an SESE (???
   * A node can contain one of three things:
   *  - a single instruction
   *  - a group of instructions (not necessarily a basic block)
   *  - a list of child nodes
   * If a node contains child nodes, then it cannot contain instructions itself.
   * Thus, only leaf nodes may contain instructions.
   */
  struct SESENode
  {
    public:
      // NOTE(greg): Currently only using SESE
      enum class NodeType { LOOP, SESE };

      SESENode();
      SESENode(BasicBlock *);

      void setParent(SESENode *);
      void setDepth(int);
      void addChild(SESENode *);
      void addInstruction(Instruction *);
      void clearInstructions();
      void setAnnotation(std::pair<std::string, std::string>);
      std::vector<SESENode *> getChildren();
      std::vector<Instruction *> getInstructions();
      std::unordered_map<std::string, std::string> &getAnnotation( void );

      std::vector<Instruction *> findSplitPoints();

      friend std::ostream &operator<<(std::ostream &, const SESENode &);

    private:
      bool is_leaf;

      /*
       * Depth 0 is root of the tree
       */
      int depth;

      SESENode *parent;

      /*
       * Children of this node. Is empty if leaf.
       */
      std::vector<SESENode *> children;

      /*
       * Instruction of node. Is empty if not leaf.
       */
      std::vector<Instruction *> instructions;

      /*
       * Annotations pertaining to a node. Children inherit annotations of parent unlesss
       * overriden.
       * NOTE(greg): do we need a notion of priority?
       */
      std::unordered_map<std::string, std::string> annotations;

      /*
       * NOTE(greg): might be useless later on. May be nullptr.
       */
      BasicBlock *basic_block;
  };

  /*
   * Algorithm for constructing annotated tree:
   *  1. Root of tree is the function. For each basic block, add it as a leaf node
   *     with the root as its parent.
   *
   *  2. Look for annotations starting from the outermost scope and group nodes by
   *     these annotations, creating an intermediate node for each annotation.
   *     a. If there are multiple annotations within a basic block, it will get split
   *        into two basic blocks.
   *
   *  E.g. if we have the following code:
   *
   *  int main(void)
   *  {
   *    #pragma noelle note independent = 1
   *    for ( int i = 0; i < N; i++ ) {
   *      #pragma noelle note ordered = 1
   *      printf("ordered");
   *
   *      #pragma noelle note unordered = 1
   *      printf("unordered");
   *    }
   *  }
   *
   *  The tree would start with the basic each basic block as a node under `main`
   *
   * main ___ entry
   *      |__ for.cond
   *      |__ for.body
   *      |__ for.end
   *
   * Then, an intermediate node gets created for the "independent = 1" annotation
   *
   * main ___ entry
   *      |__ "independent = 1" ___ for.cond
   *                            |__ for.body
   *                            |__ for.end
   *
   * Lastly, the another intermediate node gets created for the "ordered = 1" annotation
   * NOTE(greg): Verify that this does not get assigned to the for.end block too
   *
   * main ___ entry
   *      |__ "independent = 1" ___ for.cond
   *                            |__ for.end
   *                            |__ "ordered = 1"   ___ for.body_ordered
   *                            |__ "unordered = 1" ___ for.body_unordered
   */
} // namespace llvm
