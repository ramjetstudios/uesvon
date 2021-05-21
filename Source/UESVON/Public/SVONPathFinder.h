#pragma once

#include "SVONLink.h"
#include "SVONNavigationPath.h"
#include "UESVON/Public/SVONTypes.h"
#include "SVONPathFinder.generated.h"

USTRUCT(BlueprintType)
struct FSVONPathFinderSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	bool myDebugOpenNodes = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	bool myUseUnitCost = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	float myUnitCost = 1.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	float myEstimateWeight = 1.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	float myNodeSizeCompensation = 1.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	int mySmoothingIterations = 0.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	ESVONPathCostType myPathCostType = ESVONPathCostType::Euclidean;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SVON")
	TArray<FVector> myDebugPoints;
};

USTRUCT(BlueprintType)
struct UESVON_API FSVONPathFinder
{
	GENERATED_BODY()
	
	FSVONPathFinder() {}
	FSVONPathFinder(ASVONVolume* aVolume, FSVONPathFinderSettings& aSettings) : Volume(aVolume), mySettings(aSettings) {}

	/* Performs an A* search from start to target navlink */
	int FindPath(const FSVONLink& aStart, const FSVONLink& aTarget, const FVector& aStartPos, const FVector& aTargetPos, FSVONNavPathSharedPtr* oPath);

private:
	TArray<FSVONLink> myOpenSet;
	TSet<FSVONLink> myClosedSet;

	TMap<FSVONLink, FSVONLink> myCameFrom;

	TMap<FSVONLink, float> myGScore;
	TMap<FSVONLink, float> myFScore;

	FSVONLink myStart;
	FSVONLink myCurrent;
	FSVONLink myGoal;

	UPROPERTY(VisibleInstanceOnly, Category="SVON")
	ASVONVolume* Volume = nullptr;

	FSVONPathFinderSettings mySettings;

	/* A* heuristic calculation */
	float HeuristicScore(const FSVONLink& aStart, const FSVONLink& aTarget);

	/* Distance between two links */
	float GetCost(const FSVONLink& aStart, const FSVONLink& aTarget);

	void ProcessLink(const FSVONLink& aNeighbour);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(TMap<FSVONLink, FSVONLink>& aCameFrom, FSVONLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FSVONNavPathSharedPtr* oPath);

	/*void Smooth_Chaikin(TArray<FVector>& somePoints, int aNumIterations);*/
};
