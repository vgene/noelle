#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Tree.hpp"

using namespace llvm;

namespace llvm
{

FunctionTree::FunctionTree()
{
  root = nullptr;
}

FunctionTree::FunctionTree(Function *f) : FunctionTree()
{
  associated_function = f;
}

bool FunctionTree::splitNodesRecursive(SESENode *node)
{
  for ( auto child : node->getChildren() )
  {
    std::vector<Instruction *> split_points = child->findSplitPoints();

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
      while ( *current_inst != split )
      {
        added_node->addInstruction( *current_inst );
        current_inst++;
      }
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
  this->root = new SESENode();
  root->setDepth( 0 );
  root->setParent( nullptr );

  // add all basic blocks as children of root
  for ( auto &bb : *associated_function )
  {
    SESENode *node = new SESENode( &bb );
    node->setParent( root );
    node->setDepth( 1 );
    root->addChild( node );
  }

  splitNodesRecursive( root );

  return modified;

#if 0
  // NOTE(greg): probably make this recursive
  for ( auto node : root->getChildren() )
  {
    std::vector<Instruction *> split_points = node->findSplitPoints();

    if ( split_points.size() == 0 )
      continue;

    // TODO(greg): Split basic block and change tree
    //  1. Create nodes with instructions from before and after split points
    //  2. Remove instructions from current node
    //  3. Set children of current node to be nodes created in (1)
    auto current_inst = node->getInstructions().begin();
    for ( auto split : split_points )
    {
      SESENode *added_node = new SESENode();
      while ( *current_inst != split )
      {
        added_node->addInstruction( *current_inst );
        current_inst++;
      }
      node->addChild( added_node );
    }

    // need to add instructions after last split
    SESENode *added_node = new SESENode();
    while ( current_inst != node->getInstructions().end() )
    {
      added_node->addInstruction( *current_inst );
      current_inst++;
    }
    node->addChild( added_node );

    // clear instructions from intermediate node
    node->clearInstructions();

    // call the recursive function here??
  }
#endif
}

std::ostream &operator<<(std::ostream &os, const FunctionTree &tree)
{
}

bool FunctionTree::insertSplits()
{
  MDNode *last_meta_node = nullptr;
  bool has_meta = false;
  std::vector<Instruction *> split_points;

  for ( auto &bb : *associated_function )
  {
    for ( auto &inst : bb )
    {
      has_meta = inst.hasMetadata();

      // if metadata changed between instructions, split basic block
      if ( (!has_meta && last_meta_node != nullptr ) ||
           (inst.getMetadata("note.noelle") != last_meta_node) )
      {
        // not first or last instruction in basic block
        if ( &inst != &*bb.begin() && &inst != &*std::prev(bb.end()) )
          split_points.push_back( &inst );

        if ( has_meta )
        {
          last_meta_node = inst.getMetadata("note.noelle");
          last_meta_node->dump();
        }
      }
    }
  }

  if ( split_points.size() == 0 )
    return false;

  for ( auto inst : split_points )
    SplitBlock( inst->getParent(), inst );

  return true;
}

} // namespace llvm
