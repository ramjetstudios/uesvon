#pragma once

#include "UESVON/Private/libmorton/morton.h"
#include "UESVON/Public/SVONDefines.h"

struct UESVON_API SVONLeafNode
{
	uint64 myVoxelGrid = 0;

	inline bool GetNodeAt(uint32 aX, uint32 aY, uint32 aZ) const
	{
		const uint64 index = libmorton::morton3D_64_encode(aX, aY, aZ);
		return (myVoxelGrid & (1ULL << index)) != 0;
	}

	inline void SetNodeAt(uint32 aX, uint32 aY, uint32 aZ)
	{
		const uint64 index = libmorton::morton3D_64_encode(aX, aY, aZ);
		myVoxelGrid |= 1ULL << index;
	}

	inline void SetNode(uint8 aIndex)
	{
		myVoxelGrid |= 1ULL << aIndex;
	}

	inline bool GetNode(mortoncode_t aIndex) const
	{
		return (myVoxelGrid & (1ULL << aIndex)) != 0;
	}

	inline bool IsCompletelyBlocked() const
	{
		return myVoxelGrid == -1;
	}

	inline bool IsEmpty() const
	{
		return myVoxelGrid == 0;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, SVONLeafNode& aSVONLeafNode)
{
	Ar << aSVONLeafNode.myVoxelGrid;
	return Ar;
}
