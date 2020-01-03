#include "Node.hpp"
#include <map>
#include <unordered_map>

using namespace llvm;

namespace llvm
{

  SESENode::SESENode()
  {
    is_leaf = true;
    depth = -1;

    parent = nullptr;
    basic_block = nullptr;
  }

  SESENode::SESENode(BasicBlock *bb) : SESENode()
  {
    basic_block = bb;

    // do we want to do this here or split it as a separate function
    for ( auto &inst : *bb )
      instructions.push_back( &inst );
  }

  void SESENode::setParent(SESENode *node)
  {
    parent = node;
  }

  void SESENode::setDepth(int d)
  {
    depth = d;
  }

  void SESENode::addChild(SESENode *node)
  {
    this->is_leaf = false;
    children.push_back( node );
  }

  std::vector<SESENode *> SESENode::getChildren()
  {
    return this->children;
  }

  void SESENode::addInstruction(Instruction *i)
  {
    instructions.push_back( i );
  }

  void SESENode::clearInstructions()
  {
    this->instructions.clear();
  }

  void SESENode::setAnnotation(std::pair<std::string, std::string> annot)
  {
  }

  std::vector<Instruction *> SESENode::getInstructions()
  {
    return this->instructions;
  }

  std::unordered_map<std::string, std::string> &SESENode::getAnnotation( void )
  {
    return this->annotations;
  }

  std::vector<Instruction *> SESENode::findSplitPoints()
  {
    MDNode *last_meta_node = nullptr;
    bool has_meta = false;
    std::vector<Instruction *> split_points;
    for ( auto &inst : *basic_block )
    {
      has_meta = inst.hasMetadata();

      // if metadata changed between instructions, split basic block
      if ( (!has_meta && last_meta_node != nullptr ) ||
           (inst.getMetadata("note.noelle") != last_meta_node) )
      {
        // not first or last instruction in basic block
        if ( &inst != &*basic_block->begin() && &inst != &*std::prev(basic_block->end()) )
          split_points.push_back( &inst );

        if ( has_meta )
        {
          last_meta_node = inst.getMetadata("note.noelle");
          last_meta_node->dump();
        }
      }
    }

    return split_points;
  }

  std::ostream &operator<<(std::ostream &os, const SESENode &node)
  {
    if ( !node.is_leaf )
    {
      if ( !node.parent )
        os << "** Root node **\n";
      else
        os << "-- Intermediate node --\n";
    }
    else
      os << "++ Leaf node ++\n";

    os << "\tAnnotations\n";
    for ( auto &annot : node.annotations )
    {
      os << annot.first << " : " << annot.second << "\n";
    }

    return os;
  }

} // namespace llvm
