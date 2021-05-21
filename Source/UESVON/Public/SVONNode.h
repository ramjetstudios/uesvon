#pragma once

#include "CoreMinimal.h"
#include "SVONLink.h"
#include "SVONNode.generated.h"

USTRUCT(BlueprintType)
struct FSVONNode
{
	GENERATED_BODY()
	
	uint64 myCode;

	FSVONLink myParent;
	FSVONLink myFirstChild;

	FSVONLink myNeighbours[6];

	FSVONNode() :
		myCode(0),
		myParent(FSVONLink::GetInvalidLink()),
		myFirstChild(FSVONLink::GetInvalidLink()) {}

	bool HasChildren() const { return myFirstChild.IsValid(); }

};

FORCEINLINE FArchive &operator <<(FArchive &Ar, FSVONNode& aSVONNode)
{
	Ar << aSVONNode.myCode;
	Ar << aSVONNode.myParent;
	Ar << aSVONNode.myFirstChild;

	for (int i = 0; i < 6; i++)
	{
		Ar << aSVONNode.myNeighbours[i];
	}

	return Ar;
}