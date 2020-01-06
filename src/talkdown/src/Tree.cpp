#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Tree.hpp"

#include <iostream>

using namespace llvm;

namespace llvm
{

FunctionTree::FunctionTree()
{
  root = nullptr;
}

FunctionTree::~FunctionTree()
{

}

FunctionTree::FunctionTree(Function *f) : FunctionTree()
{
  associated_function = f;
}

bool FunctionTree::splitNodesRecursive(SESENode *node)
{
  for ( auto child : node->getChildren() )
  {
    // std::vector<Instruction *> split_points = child->findSplitPoints();
    auto split_points = child->findSplitPoints();
    /* std::cerr << "Split points for " << node << ":\n"; */
    /* for ( auto &it : split_points ) */
    /*   std::cerr << "\t" << *(it.first) << "\n"; */

    if ( split_points.size() == 0 )
      continue;

    // TODO(greg): Split basic block and change tree
    //  1. Create nodes with instructions from before and after split points
    //  2. Remove instructions from current node
    //  3. Set children of current node to be nodes created in (1)
    auto current_inst = child->getInstructions().begin();
    for ( auto split : split_points )
    {
      SESENode *added_node = new SESENode();
      while ( *current_inst != split.first )
      {
        added_node->addInstruction( *current_inst );
        current_inst++;
      }
      child->addAnnotations(split.second);
      child->addChild( added_node );
    }

    // need to add instructions after last split
    SESENode *added_node = new SESENode();
    while ( current_inst != child->getInstructions().end() )
    {
      added_node->addInstruction( *current_inst );
      current_inst++;
    }
    child->addChild( added_node );

    // clear instructions from intermediate node
    child->clearInstructions();

    // call the recursive function here??
    for ( auto childchild : child->getChildren() )
      splitNodesRecursive( childchild );
  }

  return false;
}

bool FunctionTree::constructTree(Function *f)
{
  bool modified = false;

  // if we are using instructions instead of basic blocks as regions
  // then this wont work...
  // modified |= insertSplits();

  // construct root node
  // TODO(greg): with annotation of function if there is one
  this->root = new SESENode();
  root->setDepth( 0 );
  root->addAnnotation(std::pair<std::string, std::string>("Root", ""));
  root->setParent( nullptr );

  // add all basic blocks as children of root
  for ( auto &bb : *associated_function )
  {
    SESENode *node = new SESENode( &bb );
    node->setParent( root );
    node->setDepth( 1 );
    root->addChild( node );
  }

  // split children nodes recursively until all annotations are split
  splitNodesRecursive( root );

  return modified;
}

std::ostream &operator<<(std::ostream &os, const FunctionTree &tree)
{
  os << "Function: " << tree.associated_function->getName().str() << "\n";
  return tree.root->recursivePrint( os );
}

} // namespace llvm
