#include "Client/Taintness/Analysis/TaintAnalysis.h"
#include "Client/Taintness/DataFlow/TaintAnalysisEngine.h"
#include "Client/Taintness/DataFlow/TaintGlobalState.h"
#include "Client/Taintness/SourceSink/SinkViolationChecker.h"

#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace tpa;

namespace client
{
namespace taint
{

static void printSinkViolation(const ProgramLocation& pLoc, const std::vector<SinkViolationRecord>& records)
{
	for (auto const& record: records)
	{
		errs().changeColor(raw_ostream::RED);

		errs() << "\nSink violation at " << *pLoc.getContext() << ":: " << *pLoc.getInstruction() << "\n";
		errs() << "\tArgument: " << record.argPos << "\n";
		errs() << "\tExpected: " << record.expectVal << "\n";
		errs() << "\tActual:   " << record.actualVal << "\n";

		errs().resetColor();
	}
}

TaintAnalysis::TaintAnalysis(const tpa::PointerAnalysis& p, const ExternalPointerEffectTable& t): ptrAnalysis(p), extTable(t)
{
	sourceSinkLookupTable.readSummaryFromFile("source_sink.conf");
}

bool TaintAnalysis::checkSinkViolation(const TaintGlobalState& globalState)
{
	bool ret = false;
	for (auto const& sinkSignature: globalState.getSinkSignatures())
	{
		auto optStore = globalState.getMemo().lookup(sinkSignature.getCallSite());
		auto const& store = (optStore == nullptr) ? TaintStore() : *optStore;

		auto checkResult = SinkViolationChecker(globalState.getEnv(), store, sourceSinkLookupTable, ptrAnalysis).checkSinkViolation(sinkSignature);

		if (!checkResult.empty())
		{
			ret = true;
			printSinkViolation(sinkSignature.getCallSite(), checkResult);
		}
	}
	return ret;
}

// Return true if there is a info flow violation
bool TaintAnalysis::runOnDefUseModule(const DefUseModule& duModule)
{
	TaintGlobalState globalState(duModule, ptrAnalysis, extTable, sourceSinkLookupTable);
	TaintAnalysisEngine engine(globalState);
	engine.run();

	return checkSinkViolation(globalState);
}

}
}