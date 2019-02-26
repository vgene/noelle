
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>


#include <fstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>
#include <VIAInvoker.hpp>

#include "VIAInvoker.hpp"

cl::opt<std::string> OracleExecutionScriptPath("exe-script",
    cl::desc("Path to the script used to instrument and execute the current IR"),
    cl::value_desc("filename"), cl::ValueRequired);

using namespace oracle_aa;

const StringRef VIAInvoker::RAR = "RAR";
const StringRef VIAInvoker::RAW = "RAW";
const StringRef VIAInvoker::WAR = "WAR";
const StringRef VIAInvoker::WAW = "WAW";

oracle_aa::VIAInvoker::VIAInvoker(Module &M, ModulePass& MP) : M(M), MP(MP) {
  moduleID = UniqueIRMarkerReader::getModuleID(&M);
  auto modIDStr = std::to_string(moduleID);

  results = std::make_shared<OracleAliasResults>();

  viaConfigFilename = modIDStr + "-oracle-ddg.viaconf";
  moduleBitcodeFilename = modIDStr + ".bc";
  viaResultFilename = modIDStr + "-oracle-ddg.dep";
}

void oracle_aa::VIAInvoker::runInference(StringRef inputArgs) {
  SmallVector<Loop *, 8> sm{};
  buildOracleDDGConfig(sm);
  dumpModule();
  executeVIAInference(inputArgs);
  parseResponse();
}

// build a viaconf file for each loop in the program and write it to viaConfigFilename
void oracle_aa::VIAInvoker::buildOracleDDGConfig(llvm::SmallVector<llvm::Loop *, 8> lp) {

//  std::vector<uint64_t> loopIDs{};

//  for ( auto &F: M ) {
//    auto &LI = MP.getAnalysis<llvm::LoopInfoWrapperPass>(F).getLoopInfo();
//    for ( auto &L : LI ) {
//      auto *metaNode = L->getLoopID();
//      assert(metaNode && "No metadata");
//      assert(metaNode->getNumOperands() == 2 && "Loop ID must have two operands (itself and the ID");
//      auto *constant = dyn_cast<ConstantAsMetadata>(metaNode->getOperand(1))->getValue();
//      auto v = dyn_cast<ConstantInt>(constant)->getZExtValue();
//      loopIDs.push_back(v);
//      errs() << "loop id: " << v << '\n';
//    }
//  }


  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> prettyBuffer(buffer);


  prettyBuffer.StartObject();
    prettyBuffer.Key("Monitor"); prettyBuffer.String("Basic");
    prettyBuffer.Key("Model"); prettyBuffer.String("OracleDDG");
    prettyBuffer.Key("Loop"); prettyBuffer.String("all");
  prettyBuffer.EndObject();

  std::ofstream ofs(viaConfigFilename);

  ofs << buffer.GetString() << std::endl;

}

// write the current IR module with (unique Value ids) to a bitcode file
// in the same dir the viaconf file.
void oracle_aa::VIAInvoker::dumpModule() {

  std::error_code EC;

  errs() << moduleBitcodeFilename << '\n';

  llvm::raw_fd_ostream OS(moduleBitcodeFilename, EC, sys::fs::OpenFlags::F_None);
  WriteBitcodeToFile(&M, OS);
  OS.flush();

  if(EC) {
    errs() << EC.message() << "\n";
    assert(0 && "EC Failed");
  }
}

// run opt (from a bash script) to instrument the saved module.
// then create an executable from this
// execute the executable
void oracle_aa::VIAInvoker::executeVIAInference(StringRef inputArgs) {

  auto ScriptWithArgs = OracleExecutionScriptPath.getValue() + " " + std::to_string(moduleID) + " " + inputArgs.str();

  errs() << ScriptWithArgs << '\n';

  auto returnValue = system(ScriptWithArgs.c_str());
  if (returnValue) {
    errs() << "Return Value: " << returnValue << '\n';
    assert(0 && "Cannot continue with pass since the execution of the script failed");
  }
}

// read the result from the executable which is of the form <moduleID>-oracle-ddg.dep
// create a OracleAliasResult (a quad of all types of memory dependencies).
void oracle_aa::VIAInvoker::parseResponse() {

  errs() << viaResultFilename << '\n';
  std::ifstream ifs(viaResultFilename);

  if( !ifs.good() ) {
    assert(0 && "ifs not good");
  }

  rapidjson::Document doc;
  rapidjson::IStreamWrapper isw(ifs);
  doc.ParseStream(isw);

  // Find all Dependency IDs
  std::set<IDType> ids;
  auto &resultList = doc["Result"];
  for ( auto &iter : resultList.GetArray() ) {
    for ( auto &dep : iter[DependenciesKey].GetArray()) {
      auto first = dep[1].GetUint64();
      auto second = dep[2].GetUint64();
      ids.insert(first);
      ids.insert(second);
    }
  }

  // Create an ID to Dependency Mapping.
  IDToValueMapper idToValueMapper(M);
  auto mapping = idToValueMapper.idToValueMap(ids);

  auto module = doc["Module"].GetUint64();
  for ( auto &iter : resultList.GetArray() ) {
    auto function = iter["Function"].GetUint64();
    for ( auto &dep : iter[DependenciesKey].GetArray() ) {
      auto depType = StringRef(dep[0].GetString());
      auto first = dep[1].GetUint64();
      auto firstValue = dyn_cast<Instruction>((*mapping)[first]);
      assert(firstValue);
      auto second = dep[2].GetUint64();
      auto secondValue = dyn_cast<Instruction>((*mapping)[second]);
      assert(secondValue);
      auto dependency = std::pair<const Value *, const Value *>(getPtrValue(firstValue), getPtrValue(secondValue));
      if (depType == RAR) {
        results->addFunctionRaR(module, function, dependency);
      } else if (depType == RAW) {
        results->addFunctionRaW(module, function, dependency);
      } else if (depType == WAR) {
        results->addFunctionWaR(module, function, dependency);
      } else if (depType == WAW) {
        results->addFunctionWaW(module, function, dependency);
      } else {
        errs() << "depType not known: " << depType << '\n';
        assert(0 && "found an unknown dependency type");
      }
      errs() << "parsing response: ";
      getPtrValue(firstValue)->print(errs());
      errs() << " ";
      getPtrValue(secondValue)->print(errs());
      errs() << '\n';
    }
  }

}

Value *VIAInvoker::getPtrValue(Instruction *I) {
  if (auto *Load = dyn_cast<LoadInst>(I)) {
    return Load->getPointerOperand();
  } else if (auto *Store = dyn_cast<StoreInst>(I)) {
    return Store->getPointerOperand();
  } else {
    errs() << "Got an Instruction with opname: " <<  I->getOpcodeName() << '\n';
    assert ( 0 && "Instruction must be a load or a store" );
  }
}

std::shared_ptr<OracleAliasResults> VIAInvoker::getResults() {
  assert( results && "have not yet computed AA results" );
  return results;
}
