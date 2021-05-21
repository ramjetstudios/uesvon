#include "UESVON/Public/SVONPathFinder.h"
#include "UESVON/Public/SVONLink.h"
#include "UESVON/Public/SVONNode.h"
#include "UESVON/Public/SVONVolume.h"
#include "UESVON.h"
#include "SVONNavigationPath.h"

int FSVONPathFinder::FindPath(const FSVONLink& aStart, const FSVONLink& aGoal, const FVector& aStartPos, const FVector& aTargetPos, FSVONNavPathSharedPtr* oPath)
{
	myOpenSet.Empty();
	myClosedSet.Empty();
	myCameFrom.Empty();
	myFScore.Empty();
	myGScore.Empty();
	myCurrent = FSVONLink();
	myGoal = aGoal;
	myStart = aStart;

	myOpenSet.Add(aStart);
	myCameFrom.Add(aStart, aStart);
	myGScore.Add(aStart, 0);
	myFScore.Add(aStart, HeuristicScore(aStart, myGoal)); // Distance to target

	int numIterations = 0;

	while (myOpenSet.Num() > 0)
	{
		float lowestScore = FLT_MAX;
		for (FSVONLink& link : myOpenSet)
		{
			if (!myFScore.Contains(link) || myFScore[link] < lowestScore)
			{
				lowestScore = myFScore[link];
				myCurrent = link;
			}
		}

		myOpenSet.Remove(myCurrent);
		myClosedSet.Add(myCurrent);

		if (myCurrent == myGoal)
		{
			BuildPath(myCameFrom, myCurrent, aStartPos, aTargetPos, oPath);
#if WITH_EDITOR
			UE_LOG(UESVON, Display, TEXT("Pathfinding complete, iterations : %i"), numIterations);
#endif
			return 1;
		}

		const FSVONNode& currentNode = Volume->GetNode(myCurrent);

		TArray<FSVONLink> neighbours;

		if (myCurrent.GetLayerIndex() == 0 && currentNode.myFirstChild.IsValid())
		{
			Volume->GetLeafNeighbours(myCurrent, neighbours);
		}
		else
		{
			Volume->GetNeighbours(myCurrent, neighbours);
		}

		for (const FSVONLink& neighbour : neighbours)
		{
			ProcessLink(neighbour);
		}

		numIterations++;
	}
#if WITH_EDITOR
	UE_LOG(UESVON, Display, TEXT("Pathfinding failed, iterations : %i"), numIterations);
#endif
	return 0;
}

float FSVONPathFinder::HeuristicScore(const FSVONLink& aStart, const FSVONLink& aTarget)
{
	/* Just using manhattan distance for now */
	float score;

	FVector startPos, endPos;
	Volume->GetLinkPosition(aStart, startPos);
	Volume->GetLinkPosition(aTarget, endPos);
	switch (mySettings.myPathCostType)
	{
	case ESVONPathCostType::Manhattan: score = FMath::Abs(endPos.X - startPos.X) + FMath::Abs(endPos.Y - startPos.Y) + FMath::Abs(endPos.Z - startPos.Z);
		break;
	case ESVONPathCostType::Euclidean:
	default: score = (startPos - endPos).Size();
		break;
	}

	score *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(Volume->GetMyNumLayers())) * mySettings.myNodeSizeCompensation);

	return score;
}

float FSVONPathFinder::GetCost(const FSVONLink& aStart, const FSVONLink& aTarget)
{
	float cost;

	// Unit cost implementation
	if (mySettings.myUseUnitCost)
	{
		cost = mySettings.myUnitCost;
	}
	else
	{
		FVector startPos(0.f), endPos(0.f);
		Volume->GetLinkPosition(aStart, startPos);
		Volume->GetLinkPosition(aTarget, endPos);
		cost = (startPos - endPos).Size();
	}

	cost *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(Volume->GetMyNumLayers())) * mySettings.myNodeSizeCompensation);

	return cost;
}

void FSVONPathFinder::ProcessLink(const FSVONLink& aNeighbour)
{
	if (aNeighbour.IsValid())
	{
		if (myClosedSet.Contains(aNeighbour))
			return;

		if (!myOpenSet.Contains(aNeighbour))
		{
			myOpenSet.Add(aNeighbour);

			if (mySettings.myDebugOpenNodes)
			{
				FVector pos;
				Volume->GetLinkPosition(aNeighbour, pos);
				mySettings.myDebugPoints.Add(pos);
			}
		}

		float t_gScore = FLT_MAX;
		if (myGScore.Contains(myCurrent))
			t_gScore = myGScore[myCurrent] + GetCost(myCurrent, aNeighbour);
		else
			myGScore.Add(myCurrent, FLT_MAX);

		if (t_gScore >= (myGScore.Contains(aNeighbour) ? myGScore[aNeighbour] : FLT_MAX))
			return;

		myCameFrom.Add(aNeighbour, myCurrent);
		myGScore.Add(aNeighbour, t_gScore);
		myFScore.Add(aNeighbour, myGScore[aNeighbour] + (mySettings.myEstimateWeight * HeuristicScore(aNeighbour, myGoal)));
	}
}

void FSVONPathFinder::BuildPath(TMap<FSVONLink, FSVONLink>& aCameFrom, FSVONLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FSVONNavPathSharedPtr* oPath)
{
	FSVONPathPoint pos;

	TArray<FSVONPathPoint> points;

	if (!oPath || !oPath->IsValid())
		return;

	while (aCameFrom.Contains(aCurrent) && !(aCurrent == aCameFrom[aCurrent]))
	{
		aCurrent = aCameFrom[aCurrent];
		Volume->GetLinkPosition(aCurrent, pos.myPosition);
		points.Add(pos);
		const FSVONNode& node = Volume->GetNode(aCurrent);
		// This is rank. I really should sort the layers out
		if (aCurrent.GetLayerIndex() == 0)
		{
			if (!node.HasChildren())
				points[points.Num() - 1].myLayer = 1;
			else
				points[points.Num() - 1].myLayer = 0;
		}
		else
		{
			points[points.Num() - 1].myLayer = aCurrent.GetLayerIndex() + 1;
		}
	}

	if (points.Num() > 1)
	{
		points[0].myPosition = aTargetPos;
		points[points.Num() - 1].myPosition = aStartPos;
	}
	else // If start and end are in the same voxel, just use the start and target positions.
	{
		if (points.Num() == 0)
			points.Emplace();

		points[0].myPosition = aTargetPos;
		points.Emplace(aStartPos, myStart.GetLayerIndex());
	}

	for (int i = points.Num() - 1; i >= 0; i--)
	{
		oPath->Get()->GetPathPoints().Add(points[i]);
	}
}
