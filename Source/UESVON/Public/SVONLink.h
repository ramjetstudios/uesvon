#pragma once
#include "SVONDefines.h"

struct UESVON_API SVONLink
{
	layerindex_t myLayerIndex:4;
	nodeindex_t myNodeIndex:22;
	subnodeindex_t mySubnodeIndex:6;

	SVONLink()
		: myLayerIndex(15),
		  myNodeIndex(0),
		  mySubnodeIndex(0)
	{
	}

	SVONLink(layerindex_t aLayer, nodeindex_t aNodeIndex, subnodeindex_t aSubNodeIndex)
		: myLayerIndex(aLayer),
		  myNodeIndex(aNodeIndex),
		  mySubnodeIndex(aSubNodeIndex)
	{
	}

	layerindex_t GetLayerIndex() const
	{
		return myLayerIndex;
	}

	void SetLayerIndex(const layerindex_t aLayerIndex)
	{
		myLayerIndex = aLayerIndex;
	}

	nodeindex_t GetNodeIndex() const
	{
		return myNodeIndex;
	}

	void SetNodeIndex(const nodeindex_t aNodeIndex)
	{
		myNodeIndex = aNodeIndex;
	}

	subnodeindex_t GetSubnodeIndex() const
	{
		return mySubnodeIndex;
	}

	void SetSubnodeIndex(const subnodeindex_t aSubnodeIndex)
	{
		mySubnodeIndex = aSubnodeIndex;
	}

	bool IsValid() const
	{
		return myLayerIndex != 15;
	}

	void SetInvalid()
	{
		myLayerIndex = 15;
	}

	bool operator==(const SVONLink& aOther) const
	{
		return memcmp(this, &aOther, sizeof(SVONLink)) == 0;
	}

	static SVONLink GetInvalidLink()
	{
		return SVONLink(15, 0, 0);
	}

	FString ToString()
	{
		return FString::Printf(TEXT("%i:%i:%i"), myLayerIndex, myNodeIndex, mySubnodeIndex);
	};
};

FORCEINLINE uint32 GetTypeHash(const SVONLink& b)
{
	return HashCombine(HashCombine(GetTypeHash(b.myLayerIndex), GetTypeHash(b.myNodeIndex)), GetTypeHash(b.mySubnodeIndex));
}


FORCEINLINE FArchive& operator <<(FArchive& Ar, SVONLink& aSVONLink)
{
	Ar.Serialize(&aSVONLink, sizeof(SVONLink));
	return Ar;
}
