#include "PointerAnalysis/Engine/GlobalState.h"
#include "PointerAnalysis/Engine/StorePruner.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/MemoryObject.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"
#include "PointerAnalysis/Program/CFG/CFGNode.h"

namespace tpa
{

static bool isAccessible(const MemoryObject* obj)
{
	return !(obj->isStackObject() || obj->isHeapObject());
}

StorePruner::ObjectSet StorePruner::getRootSet(const Store& store, const ProgramPoint& pp)
{
	ObjectSet ret;

	auto ctx = pp.getContext();
	auto const& func = pp.getCFGNode()->getFunction();
	for (auto& argVal: func.args())
	{
		if (!argVal.getType()->isPointerTy())
			continue;
		auto argPtr = ptrManager.getPointer(ctx, &argVal);
		assert(argPtr != nullptr);

		auto argSet = env.lookup(argPtr);
		ret.insert(argSet.begin(), argSet.end());
	}

	for (auto const& mapping: store)
	{
		if (isAccessible(mapping.first))
			ret.insert(mapping.first);
	}

	return ret;
}

void StorePruner::findAllReachableObjects(const Store& store, ObjectSet& reachableSet)
{
	std::vector<const MemoryObject*> currWorkList(reachableSet.begin(), reachableSet.end());
	std::vector<const MemoryObject*> nextWorkList;
	nextWorkList.reserve(reachableSet.size());
	ObjectSet exploredSet;

	while (!currWorkList.empty())
	{
		for (auto obj: currWorkList)
		{
			if (!exploredSet.insert(obj).second)
				continue;

			reachableSet.insert(obj);

			auto offsetObjs = memManager.getReachablePointerObjects(obj, false);
			for (auto oObj: offsetObjs)
				if (!exploredSet.count(oObj))
					nextWorkList.push_back(oObj);
				else
					break;

			auto pSet = store.lookup(obj);
			for (auto pObj: pSet)
				if (!exploredSet.count(pObj))
					nextWorkList.push_back(pObj);
		}

		currWorkList.clear();
		currWorkList.swap(nextWorkList);
	}
}

std::pair<Store, Store> StorePruner::splitStore(const Store& store, const ObjectSet& reachableSet)
{
	std::pair<Store, Store> ret;
	for (auto const& mapping: store)
	{
		if (reachableSet.count(mapping.first))
			ret.first.strongUpdate(mapping.first, mapping.second);
		else
			ret.second.strongUpdate(mapping.first, mapping.second);
	}
	return ret;
}

Store StorePruner::pruneStore(const Store& store, const ProgramPoint& pp)
{
	assert(pp.getCFGNode()->isEntryNode() && "Prunning can only happen on entry node!");

	Store retStore;
	Store unreachStore;
	
	auto reachableSet = getRootSet(store, pp);
	findAllReachableObjects(store, reachableSet);
	std::tie(retStore, unreachStore) = splitStore(store, reachableSet);

	auto fc = FunctionContext(pp.getContext(), &pp.getCFGNode()->getFunction());
	prunedMap[fc].mergeWith(unreachStore);

	return retStore;
}

const Store* StorePruner::lookupPrunedStore(const FunctionContext& fc)
{
	auto itr = prunedMap.find(fc);
	if (itr == prunedMap.end())
		return nullptr;
	else
		return &itr->second;
}


}