#pragma once

#include "UESVON/Public/SVONLink.h"
#include "UESVON/Public/SVONPathFinder.h"
#include "UESVON/Public/SVONTypes.h"

class FSVONFindPathTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FSVONFindPathTask>;

public:
	FSVONFindPathTask(ASVONVolume* aVolume, FSVONPathFinderSettings& aSettings, UWorld* aWorld, const FSVONLink aStart, const FSVONLink aTarget, const FVector& aStartPos, const FVector& aTargetPos, FSVONNavPathSharedPtr* oPath, FThreadSafeBool& aCompleteFlag)
		: myVolume(aVolume)
		, myWorld(aWorld)
		, myStart(aStart)
		, myTarget(aTarget)
		, myStartPos(aStartPos)
		, myTargetPos(aTargetPos)
		, myPath(oPath)
		, mySettings(aSettings)
		, myCompleteFlag(aCompleteFlag)
	{
	}

protected:
	ASVONVolume* myVolume;
	UWorld* myWorld;

	FSVONLink myStart;
	FSVONLink myTarget;
	FVector myStartPos;
	FVector myTargetPos;
	FSVONNavPathSharedPtr* myPath;

	FSVONPathFinderSettings mySettings;

	FThreadSafeBool& myCompleteFlag;

	void DoWork();

	// This next section of code needs to be here.  Not important as to why.

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSVONFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};