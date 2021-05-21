#pragma once
#include "SVONNavigationPath.generated.h"

UENUM(BlueprintType)
enum class ESVONPathCostType : uint8
{
	Manhattan,
	Euclidean
};

USTRUCT(BlueprintType)
struct FSVONPathPoint
{
	GENERATED_BODY()
	
	FSVONPathPoint()
		: myPosition(FVector())
		, myLayer(-1)
	{
	}
	FSVONPathPoint(const FVector& aPosition, int aLayer)
		: myPosition(aPosition)
		, myLayer(aLayer)
	{
	}

	FVector myPosition; // Position of the point
	int myLayer;		// Layer that the point came from (so we can infer it's volume)
};

USTRUCT(BlueprintType)
struct FSVONNavigationPath
{
	GENERATED_BODY()
	
	void AddPoint(const FSVONPathPoint& aPoint);
	void ResetForRepath();

	void DebugDraw(UWorld* aWorld, class ASVONVolume* aVolume);

	const TArray<FSVONPathPoint>& GetPathPoints() const
	{
		return myPoints;
	};

	TArray<FSVONPathPoint>& GetPathPoints() { return myPoints; }

	bool IsReady() const { return myIsReady; };
	void SetIsReady(bool aIsReady) { myIsReady = aIsReady; }

	// Copy the path positions into a standard navigation path
	void CreateNavPath(FNavigationPath& aOutPath);

protected:
	bool myIsReady = false;
	TArray<FSVONPathPoint> myPoints;
};