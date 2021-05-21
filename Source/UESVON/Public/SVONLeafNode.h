#pragma once

#include "UESVON/Private/libmorton/morton.h"
#include "SVONLeafNode.generated.h"

USTRUCT(BlueprintType)
struct FSVONLeafNode
{
	GENERATED_BODY()
	
	uint64 myVoxelGrid = 0;

	bool GetNodeAt(uint32 aX, uint32 aY, uint32 aZ) const
	{
		const uint64 index = libmorton::morton3D_64_encode(aX, aY, aZ);
		return (myVoxelGrid & (1ULL << index)) != 0;
	}

	void SetNodeAt(uint32 aX, uint32 aY, uint32 aZ)
	{
		const uint64 index = libmorton::morton3D_64_encode(aX, aY, aZ);
		myVoxelGrid |= 1ULL << index;
	}

	void SetNode(uint8 aIndex)
	{
		myVoxelGrid |= 1ULL << aIndex;
	}

	bool GetNode(uint64 aIndex) const
	{
		return (myVoxelGrid & (1ULL << aIndex)) != 0;
	}

	bool IsCompletelyBlocked() const
	{
		return myVoxelGrid == -1;
	}

	bool IsEmpty() const
	{
		return myVoxelGrid == 0;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FSVONLeafNode& aSVONLeafNode)
{
	Ar << aSVONLeafNode.myVoxelGrid;
	return Ar;
}
