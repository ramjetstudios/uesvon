#pragma once

#include "UESVON/Public/SVONLeafNode.h"
#include "UESVON/Public/SVONNode.h"
#include "SVONData.generated.h"

USTRUCT(BlueprintType)
struct FSVONData
{
	GENERATED_BODY()
	
	// SVO data
	TArray<TArray<FSVONNode>> myLayers;
	TArray<FSVONLeafNode> myLeafNodes;

	void Reset()
	{
		myLayers.Empty();
		myLeafNodes.Empty();
	}

	int GetSize() const
	{
		int result = 0;
		result += myLeafNodes.Num() * sizeof(FSVONLeafNode);
		for (int i = 0; i < myLayers.Num(); i++)
		{
			result += myLayers[i].Num() * sizeof(FSVONNode);
		}

		return result;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FSVONData& aSVONData)
{
	Ar << aSVONData.myLayers;
	Ar << aSVONData.myLeafNodes;

	return Ar;
}