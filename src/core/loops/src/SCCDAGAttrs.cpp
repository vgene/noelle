/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "SCCDAGAttrs.hpp"
#include "PDGPrinter.hpp"

using namespace llvm;
using namespace llvm::noelle;

SCCDAGAttrs::SCCDAGAttrs ()
  : enableFloatAsReal{true}, loopDG{nullptr}, sccdag{nullptr}, memoryCloningAnalysis{nullptr} {
}

SCCDAGAttrs::SCCDAGAttrs (
  bool enableFloatAsReal,
  PDG *loopDG,
  SCCDAG *loopSCCDAG,
  LoopsSummary &LIS,
  ScalarEvolution &SE,
  LoopCarriedDependencies &LCD,
  InductionVariableManager &IV,
  DominatorSummary &DS
) : 
  enableFloatAsReal{enableFloatAsReal}, loopDG{loopDG}, sccdag{loopSCCDAG}, memoryCloningAnalysis{nullptr} 
  {

  /*
   * Partition dependences between intra-iteration and iter-iteration ones.
   */
  collectLoopCarriedDependencies(LIS, LCD);

  /*
   * Collect flattened list of all IVs at all loop levels
   */
  std::set<InductionVariable *> ivs;
  std::set<InductionVariable *> loopGoverningIVs;
  for (auto &LS : LIS.loops) {
    auto loop = LS.get();
    auto loopIVs = IV.getInductionVariables(*loop);
    ivs.insert(loopIVs.begin(), loopIVs.end());
    auto loopGoverningIV = IV.getLoopGoverningInductionVariable(*loop);
    if (loopGoverningIV) loopGoverningIVs.insert(loopGoverningIV);
  }

  // DGPrinter::writeGraph<SCCDAG, SCC>("sccdag.dot", sccdag);
  // errs() << "IVs: " << ivs.size() << "\n";
  // for (auto iv : ivs) {
  //   iv->getLoopEntryPHI()->print(errs() << "IV: "); errs() << "\n";
  // }
  // errs() << "-------------\n";
  // errs() << "Loop governing IVs: " << loopGoverningIVs.size() << "\n";
  // for (auto iv : loopGoverningIVs) {
  //   iv->getLoopEntryPHI()->print(errs() << "IV: "); errs() << "\n";
  // }
  // errs() << "-------------\n";

  /*
   * Compute memory cloning location analysis
   */
  auto rootLoop = LIS.getLoopNestingTreeRoot();
  this->memoryCloningAnalysis = new MemoryCloningAnalysis(rootLoop, DS);

  /*
   * Tag SCCs depending on their characteristics.
   */
  loopSCCDAG->iterateOverSCCs([this, &SE, &LIS, &ivs, &loopGoverningIVs, &LCD](SCC *scc) -> bool {

    /*
     * Allocate the metadata about this SCC.
     */
    auto sccInfo = new SCCAttrs(scc, this->accumOpInfo, LIS);
    this->sccToInfo[scc] = sccInfo;

    /*
     * Collect information about the current SCC.
     */
    bool doesSCCOnlyContainIV = this->checkIfSCCOnlyContainsInductionVariables(scc, LIS, ivs, loopGoverningIVs);
    sccInfo->setSCCToBeInductionVariable(doesSCCOnlyContainIV);

    this->checkIfClonable(scc, SE, LIS);

    /*
     * Categorize the current SCC.
     */
    if (this->checkIfIndependent(scc)) {
      sccInfo->setType(SCCAttrs::SCCType::INDEPENDENT);

    } else if (this->checkIfReducible(scc, LIS, LCD)) {
      sccInfo->setType(SCCAttrs::SCCType::REDUCIBLE);

    } else {
      sccInfo->setType(SCCAttrs::SCCType::SEQUENTIAL);
    }

    return false;
  });

  collectSCCGraphAssumingDistributedClones();

  return ;
}

std::set<SCC *> SCCDAGAttrs::getSCCsWithLoopCarriedDependencies (void) const {
  std::set<SCC *> sccs;
  for (auto &sccDependencies : this->sccToLoopCarriedDependencies) {
    sccs.insert(sccDependencies.first);
  }
  return sccs;
}
      
std::set<SCC *> SCCDAGAttrs::getSCCsWithLoopCarriedControlDependencies (void) const {
  std::set<SCC *> sccs;

  /*
   * Iterate over SCCs with loop-carried data dependences.
   */
  for (auto &sccDependencies : this->sccToLoopCarriedDependencies) {

    /*
     * Fetch the SCC.
     */
    auto SCC = sccDependencies.first;

    /*
     * Fetch the set of loop-carried data dependences of the current SCC.
     */
    auto &deps = sccDependencies.second;

    /*
     * Check if this SCC has a control loop-carried data dependence.
     */
    auto isControl = false;
    for (auto dep : deps){
      if (dep->isControlDependence()){
        isControl = true;
        break ;
      }
    }
    if (isControl){
      sccs.insert(sccDependencies.first);
    }
  }

  return sccs;
}

std::set<SCC *> SCCDAGAttrs::getSCCsWithLoopCarriedDataDependencies (void) const {
  std::set<SCC *> sccs;

  /*
   * Iterate over SCCs with loop-carried data dependences.
   */
  for (auto &sccDependencies : this->sccToLoopCarriedDependencies) {

    /*
     * Fetch the SCC.
     */
    auto SCC = sccDependencies.first;

    /*
     * Fetch the set of loop-carried data dependences of the current SCC.
     */
    auto &deps = sccDependencies.second;

    /*
     * Check if this SCC has data loop-carried data dependence.
     */
    auto isData = false;
    for (auto dep : deps){
      if (dep->isDataDependence()){
        isData = true;
        break ;
      }
    }
    if (isData){
      sccs.insert(SCC);
    }
  }
  return sccs;
}

bool SCCDAGAttrs::isLoopGovernedBySCC (SCC *governingSCC) const {
  auto topLevelNodes = this->sccdag->getTopLevelNodes();

  /*
   * Step 1: Isolate top level SCCs (excluding independent instructions in SCCDAG)
   */
  std::queue<DGNode<SCC> *> toTraverse;
  for (auto node : topLevelNodes) {
    toTraverse.push(node);
  }
  std::set<SCC *> topLevelSCCs;
  while (!toTraverse.empty()) {

    /*
     * Fetch the current SCC and its metadata.
     */
    auto node = toTraverse.front();
    auto scc = node->getT();
    toTraverse.pop();
    auto sccInfo = this->getSCCAttrs(scc);

    if (sccInfo->canExecuteIndependently()) {
      auto nextDepth = this->sccdag->getNextDepthNodes(node);
      for (auto next : nextDepth) toTraverse.push(next);
      continue;
    }
    topLevelSCCs.insert(scc);
  }

  /*
   * Step 2: Ensure there is only 1, and that it is the target SCC
   */
  if (topLevelSCCs.size() != 1) return false;
  auto topLevelSCC = *topLevelSCCs.begin();
  return topLevelSCC == governingSCC;
}

bool SCCDAGAttrs::areAllLiveOutValuesReducable (LoopEnvironment *env) const {

  /*
   * Iterate over live-out variables.
   */
  for (auto envIndex : env->getEnvIndicesOfLiveOutVars()) {

    /*
     * Fetch the SCC that contains the producer of the environment variable.
     */
    auto producer = env->producerAt(envIndex);
    auto scc = this->sccdag->sccOfValue(producer);

    /*
     * Check the SCC type.
     */
    auto sccInfo = this->getSCCAttrs(scc);
    if (sccInfo->getType() == SCCAttrs::SCCType::INDEPENDENT) {
      continue ;
    }
    if (sccInfo->getType() == SCCAttrs::SCCType::REDUCIBLE) {
      continue ;
    }

    return false;
  }

  return true;
}

bool SCCDAGAttrs::isSCCContainedInSubloop (const LoopsSummary &LIS, SCC *scc) const {
  auto instInSubloops = true;
  auto topLoop = LIS.getLoopNestingTreeRoot();
  for (auto iNodePair : scc->internalNodePairs()) {
    if (auto inst = dyn_cast<Instruction>(iNodePair.first)) {
      instInSubloops &= (LIS.getLoop(*inst) != topLoop);
    } else {
      instInSubloops = false;
    }
  }

  return instInSubloops;
}

SCCAttrs * SCCDAGAttrs::getSCCAttrs (SCC *scc) const {
  auto sccInfo = this->sccToInfo.find(scc);
  if (sccInfo == this->sccToInfo.end()){
    return nullptr;
  }
  return sccInfo->second;
}

void SCCDAGAttrs::collectSCCGraphAssumingDistributedClones () {
  auto addIncomingNodes = [&](std::queue<DGNode<SCC> *> &queue, DGNode<SCC> *node) -> void {
    std::set<DGNode<SCC> *> nodes;
    auto scc = node->getT();
    for (auto edge : node->getIncomingEdges()) {
      nodes.insert(edge->getOutgoingNode());
      this->edgesViaClones[scc].insert(edge);
    }
    for (auto node : nodes) {
      queue.push(node);
    }
  };

  for (auto childSCCNode : this->sccdag->getNodes()) {
    auto childSCC = childSCCNode->getT();
    std::queue<DGNode<SCC> *> nodesToCheck;
    std::unordered_map<DGNode<SCC> *, bool> analyzed;

    analyzed[childSCCNode] = true;
    addIncomingNodes(nodesToCheck, childSCCNode);
    
    while (!nodesToCheck.empty()) {
      auto node = nodesToCheck.front();
      nodesToCheck.pop();
      auto scc = node->getT();
      auto sccInfo = this->getSCCAttrs(scc);
      this->parentsViaClones[childSCC].insert(scc);
      if (!sccInfo->canBeCloned()) {
        continue ;
      }
      if (analyzed[node]){
        continue ;
      }
      addIncomingNodes(nodesToCheck, node);
      analyzed[node] = true;
    }
  }

  return ;
}

void SCCDAGAttrs::collectLoopCarriedDependencies (LoopsSummary &LIS, LoopCarriedDependencies &LCD) {

  /*
   * Iterate over all the loops contained within the one handled by @this
   */
  for (auto &loop : LIS.loops) {

    /*
     * Fetch the set of loop-carried data dependences of the current loop.
     */
    auto loopCarriedEdges = LCD.getLoopCarriedDependenciesForLoop(*loop.get());

    /*
     * Make the map from SCCs to loop-carried data dependences.
     */
    for (auto edge : loopCarriedEdges) {

      /*
       * Fetch the SCCs that contain the source and destination of the current loop-carried data dependence.
       */
      auto producer = edge->getOutgoingT();
      auto consumer = edge->getIncomingT();
      auto producerSCC = this->sccdag->sccOfValue(producer);
      auto consumerSCC = this->sccdag->sccOfValue(consumer);

      /*
       * Make the mapping from SCCs to dependences explicit.
       */
      sccToLoopCarriedDependencies[producerSCC].insert(edge);
      sccToLoopCarriedDependencies[consumerSCC].insert(edge);
    }
  }

  return ;
}

bool SCCDAGAttrs::checkIfSCCOnlyContainsInductionVariables (
  SCC *scc,
  LoopsSummary &LIS,
  std::set<InductionVariable *> &IVs,
  std::set<InductionVariable *> &loopGoverningIVs) {

  /*
   * Identify contained induction variables
   */
  std::set<InductionVariable *> containedIVs;
  std::set<Instruction *> containedInsts;
  for (auto iv : IVs) {
    if (scc->isInternal(iv->getLoopEntryPHI())) {
      containedIVs.insert(iv);
      auto allInsts = iv->getAllInstructions();
      containedInsts.insert(allInsts.begin(), allInsts.end());
    }
  }
  if (containedIVs.size() == 0) return false;

  /*
   * If a contained IV is loop governing, ensure loop governance is well formed
   * TODO: Remove this, as this loop governing attribution isn't necessary for all users of SCCDAGAttrs 
   */
  for (auto containedIV : containedIVs) {
    if (loopGoverningIVs.find(containedIV) == loopGoverningIVs.end()) continue;
    auto exitBlocks = LIS.getLoop(*containedIV->getLoopEntryPHI()->getParent())->getLoopExitBasicBlocks();
    LoopGoverningIVAttribution attribution(*containedIV, *scc, exitBlocks);
    if (!attribution.isSCCContainingIVWellFormed()) {
      // containedIV->getLoopEntryPHI()->print(errs() << "Not well formed SCC for loop governing IV!\n"); errs() << "\n";
      return false;
    }
    containedInsts.insert(attribution.getHeaderCmpInst());
    containedInsts.insert(attribution.getHeaderBrInst());
    auto conditionValue = attribution.getHeaderCmpInstConditionValue();
    if (isa<Instruction>(conditionValue)) containedInsts.insert(cast<Instruction>(conditionValue));
    auto conditionDerivation = attribution.getConditionValueDerivation();
    containedInsts.insert(conditionDerivation.begin(), conditionDerivation.end());
  }

  /*
   * NOTE: No side effects can be contained in the SCC; only instructions of the IVs
   */
  for (auto nodePair : scc->internalNodePairs()) {
    auto value = nodePair.first;
    if (auto inst = dyn_cast<Instruction>(value)) {
      if (containedInsts.find(inst) != containedInsts.end()) continue;

      if (auto br = dyn_cast<BranchInst>(inst)) {
        if (br->isUnconditional()) continue;
      }

      if (isa<GetElementPtrInst>(inst) || isa<PHINode>(inst) || isa<CastInst>(inst) || isa<CmpInst>(inst)) {
        continue;
      }
    }

    // value->print(errs() << "Suspect value: "); errs() << "\n";
    // for (auto containedI : containedInsts) {
    //   containedI->print(errs() << "Contained: "); errs() << "\n";
    // }
    return false;
  }

  return true;
}

bool SCCDAGAttrs::checkIfReducible (SCC *scc, LoopsSummary &LIS, LoopCarriedDependencies &LCD) {

  /*
   * A reducible variable consists of one loop carried value
   * that tracks the evolution of the reducible value
   */
  auto rootLoop = LIS.getLoopNestingTreeRoot();
  auto rootLoopHeader = rootLoop->getHeader();
  std::unordered_set<PHINode *> loopCarriedPHIs{};
  for (auto dependency : sccToLoopCarriedDependencies.at(scc)) {

    /*
     * We do not handle reducibility of memory locations
     */
    if (dependency->isMemoryDependence()) return false;

    /*
     * Ingore external control dependencies, do not allow internal ones
     */
    auto producer = dependency->getOutgoingT();
    if (dependency->isControlDependence()) {
      if (scc->isInternal(producer)) return false;
      continue;
    }

    auto consumer = dependency->getIncomingT();
    if (!isa<PHINode>(consumer)) {
      producer->print(errs() << "Producer of LCD: "); errs() << "\n";
      consumer->print(errs() << "Consumer of LCD: "); errs() << "\n";
      cast<Instruction>(producer)->getParent()->getParent()->print(errs() << "Function\n");
    }
    assert(isa<PHINode>(consumer)
      && "All consumers of loop carried data dependencies must be PHIs");
    auto consumerPHI = cast<PHINode>(consumer);

    /*
     * Look for an internal consumer of a loop carried dependence
     *
     * NOTE: External consumers may be last-live out propagations of a reducible variable
     * or could disqualify this from reducibility: let the LoopCarriedVariable analysis determine this
     */
    if (!scc->isInternal(consumerPHI)) continue;

    /*
     * Ignore sub loops as they do not need to be reduced
     */
    if (rootLoopHeader != consumerPHI->getParent()) continue;

    loopCarriedPHIs.insert(consumerPHI);
  }

  /*
   * Check if there are loop carried dependences related to PHI nodes.
   */
  if (loopCarriedPHIs.size() != 1) {
    return false;
  }
  auto singleLoopCarriedPHI = *loopCarriedPHIs.begin();

  auto variable = new LoopCarriedVariable(*rootLoop, LCD, *loopDG, *scc, singleLoopCarriedPHI);
  if (!variable->isEvolutionReducibleAcrossLoopIterations()) {
    delete variable;
    return false;
  }

  /*
   * The SCC can be reduced.
   *
   * Check if the reducable variable is a floating point and check if floating point variables can be considered as real numbers.
   */
  auto variableType = singleLoopCarriedPHI->getType();
  if (  true
        && (variableType->isFloatTy() || variableType->isDoubleTy())
        && (!this->enableFloatAsReal)
    ){

    /*
     * Floating point values cannot be considered real numbers and therefore floating point variables cannot be reduced.
     */
    return false;
  }

  /*
   * This SCC can be reduced.
   */
  auto sccInfo = this->getSCCAttrs(scc);
  sccInfo->addLoopCarriedVariable(variable);
  return true;
}

/*
 * The SCC is independent if it doesn't have loop carried data dependencies
 */
bool SCCDAGAttrs::checkIfIndependent (SCC *scc) {
  return sccToLoopCarriedDependencies.find(scc) == sccToLoopCarriedDependencies.end();
}

void SCCDAGAttrs::checkIfClonable (SCC *scc, ScalarEvolution &SE, LoopsSummary &LIS) {

  /*
   * Check the simple cases.
   * TODO: Separate out cases and catalog SCCs by those cases
   */
  if ( false
       || isClonableByInductionVars(scc)
       || isClonableBySyntacticSugarInstrs(scc)
       || isClonableByCmpBrInstrs(scc)
       || isClonableByHavingNoMemoryOrLoopCarriedDataDependencies(scc, LIS)
      ) {
    this->getSCCAttrs(scc)->setSCCToBeClonable();
    return ;
  }

  /*
   * Check for memory cloning case
   */
  checkIfClonableByUsingLocalMemory(scc, LIS) ;

  return ;
}

void SCCDAGAttrs::checkIfClonableByUsingLocalMemory(SCC *scc, LoopsSummary &LIS) {

  /*
   * HACK: Ignore SCC without loop carried dependencies
   */
  if (this->sccToLoopCarriedDependencies.find(scc) == this->sccToLoopCarriedDependencies.end()) {
    return;
  }

  /*
   * Ensure that loop carried dependencies belong to clonable memory locations
   * NOTE: Ignore PHIs and unconditional branch instructions
   */
  std::unordered_set<const llvm::noelle::ClonableMemoryLocation *> locations;
  for (auto dependency : this->sccToLoopCarriedDependencies.at(scc)) {

    auto inst = dyn_cast<Instruction>(dependency->getOutgoingT());
    if (!inst) return;

    /*
     * Attempt to locate the instruction's clonable memory location they store/load from
     */
    auto location = this->memoryCloningAnalysis->getClonableMemoryLocationFor(inst);
    // inst->print(errs() << "Instruction: "); errs() << "\n";
    // if (!location) { 
    //   errs() << "No location\n";
    //   scc->print(errs() << "Getting close\n", "", 100); errs() << "\n";
    // }
    if (!location) return ;
    // location->getAllocation()->print(errs() << "Location found: "); errs() << "\n";
    locations.insert(location);
  }

  if (locations.size() == 0) return;

  auto sccInfo = this->sccToInfo.at(scc);
  sccInfo->setSCCToBeClonableUsingLocalMemory();
  sccInfo->addClonableMemoryLocationsContainedInSCC(locations);

  return ;
}

bool SCCDAGAttrs::isClonableByHavingNoMemoryOrLoopCarriedDataDependencies (SCC *scc, LoopsSummary &LIS) const {

  /*
   * FIXME: This check should not exist; instead, SCC where cloning
   * is trivial should be separated out by the parallelization scheme
   */
  if (this->sccdag->fetchNode(scc)->numOutgoingEdges() == 0) return false;

  for (auto edge : scc->getEdges()) {
    if (edge->isMemoryDependence()) return false;
  }

  if (sccToLoopCarriedDependencies.find(scc) == sccToLoopCarriedDependencies.end()) return true;

  auto topLoop = LIS.getLoopNestingTreeRoot();
  for (auto loopCarriedDependency : sccToLoopCarriedDependencies.at(scc)) {
    auto valueFrom = loopCarriedDependency->getOutgoingT();
    auto valueTo = loopCarriedDependency->getIncomingT();
    assert(isa<Instruction>(valueFrom) && isa<Instruction>(valueTo));
    if (LIS.getLoop(*cast<Instruction>(valueFrom)) == topLoop ||
      LIS.getLoop(*cast<Instruction>(valueTo)) == topLoop) {
        return false;
    }
  }

  return true;
}

bool SCCDAGAttrs::isClonableByInductionVars (SCC *scc) const {

  /*
   * FIXME: This check should not exist; instead, SCC where cloning
   * is trivial should be separated out by the parallelization scheme
   */
  if (this->sccdag->fetchNode(scc)->numOutgoingEdges() == 0) return false;

  /*
   * Fetch the SCC metadata.
   */
  auto sccInfo = this->getSCCAttrs(scc);

  return sccInfo->isInductionVariableSCC();
}

bool SCCDAGAttrs::isClonableBySyntacticSugarInstrs (SCC *scc) const {

  /*
   * FIXME: This check should not exist; instead, SCC where cloning
   * is trivial should be separated out by the parallelization scheme
   */
  if (this->sccdag->fetchNode(scc)->numOutgoingEdges() == 0) return false;

  if (scc->numInternalNodes() > 1) return false;
  auto I = scc->begin_internal_node_map()->first;
  if (isa<PHINode>(I) || isa<GetElementPtrInst>(I) || isa<CastInst>(I)) {
    return true;
  }
  return false;
}

bool SCCDAGAttrs::isClonableByCmpBrInstrs (SCC *scc) const {
  for (auto iNodePair : scc->internalNodePairs()) {
    auto V = iNodePair.first;
    if (auto inst = dyn_cast<Instruction>(V)){
      if (isa<CmpInst>(inst) || inst->isTerminator()) {
        continue;
      }
    }
    return false;
  }
  return true;
}

bool SCCDAGAttrs::isALoopCarriedDependence (SCC *scc, DGEdge<Value> *dependence) {

  /*
   * Fetch the set of loop-carried data dependences of a SCC.
   */
  if (this->sccToLoopCarriedDependencies.find(scc) == this->sccToLoopCarriedDependencies.end()){
    return false;
  }
  auto lcDeps = this->sccToLoopCarriedDependencies[scc];

  /*
   * Check whether the dependence is inside lcDeps.
   */
  return lcDeps.find(dependence) != lcDeps.end();
}

void SCCDAGAttrs::iterateOverLoopCarriedDataDependences (
  SCC *scc, 
  std::function<bool (DGEdge<Value> *dependence)> func
  ){

  /*
   * Iterate over internal edges of the SCC.
   */
  for (auto valuePair : scc->internalNodePairs()) {
    for (auto edge : valuePair.second->getIncomingEdges()) {

      /*
       * Check if the current edge is a loop-carried data dependence.
       */
      if (!this->isALoopCarriedDependence(scc, edge)){
        continue ;
      }

      /*
       * The current edge is a loop-carried data dependence.
       */
      auto result = func(edge);

      /*
       * Check if the caller wants us to stop iterating.
       */
      if (result){
        return ;
      }
    }
  }

  return ;
}

SCCDAG * SCCDAGAttrs::getSCCDAG (void) const {
  return this->sccdag;
}

void SCCDAGAttrs::dumpToFile (int id) {
  std::error_code EC;
  std::string filename = "sccdag-attrs-loop-" + std::to_string(id) + ".dot";

  if (EC) {
    errs() << "ERROR: Could not dump debug logs to file!";
    return ;
  }

  DG<DGString> stageGraph;
  std::set<DGString *> elements;
  std::unordered_map<DGNode<SCC> *, DGNode<DGString> *> sccToDescriptionMap;

  auto addNode = [&](std::string val, bool isInternal) -> DGNode<DGString> * {
    auto element = new DGString(val);
    elements.insert(element);
    return stageGraph.addNode(element, isInternal);
  };

  for (auto sccNode : sccdag->getNodes()) {
    std::string sccDescription;
    raw_string_ostream ros(sccDescription);

    auto sccInfo = getSCCAttrs(sccNode->getT());
    ros << "Type: ";
    if (sccInfo->canExecuteIndependently()) ros << "Independent ";
    if (sccInfo->canBeCloned()) ros << "Clonable ";
    if (sccInfo->canExecuteReducibly()) ros << "Reducible ";
    if (sccInfo->isInductionVariableSCC()) ros << "IV ";
    ros << "\n";

    for (auto iNodePair : sccNode->getT()->internalNodePairs()) {
      iNodePair.first->print(ros);
      ros << "\n";
    }

    ros.flush();
    sccToDescriptionMap.insert(std::make_pair(sccNode, addNode(sccDescription, true)));
  }

  for (auto sccEdge : sccdag->getEdges()) {
    auto outgoingDesc = sccToDescriptionMap.at(sccEdge->getOutgoingNode())->getT();
    auto incomingDesc = sccToDescriptionMap.at(sccEdge->getIncomingNode())->getT();
    stageGraph.addEdge(outgoingDesc, incomingDesc);
  }

  DGPrinter::writeGraph<DG<DGString>, DGString>(filename, &stageGraph);
  for (auto elem : elements) {
    delete elem;
  }

  return ;
}

std::unordered_set<SCCAttrs *> SCCDAGAttrs::getSCCsOfType (SCCAttrs::SCCType sccType){
  std::unordered_set<SCCAttrs *> SCCs{};

  for (auto pair : this->sccToInfo){
    auto sccAttrs = pair.second;
    if (sccAttrs->mustExecuteSequentially()){
      SCCs.insert(sccAttrs);
    }
  }

  return SCCs;
}

SCCDAGAttrs::~SCCDAGAttrs (){
  return ;
}
