#pragma once
#include "SVONDefines.h"
#include "SVONLink.generated.h"

USTRUCT(BlueprintType)
struct FSVONLink
{
	GENERATED_BODY()

	uint8 myLayerIndex:4;
	uint32 myNodeIndex:22;
	uint8 mySubnodeIndex:6;

	FSVONLink()
		: myLayerIndex(15),
		  myNodeIndex(0),
		  mySubnodeIndex(0)
	{
	}

	FSVONLink(uint8 aLayer, int32 aNodeIndex, uint8 aSubNodeIndex)
		: myLayerIndex(aLayer),
		  myNodeIndex(aNodeIndex),
		  mySubnodeIndex(aSubNodeIndex)
	{
	}

	uint8 GetLayerIndex() const
	{
		return myLayerIndex;
	}

	void SetLayerIndex(const uint8 aLayerIndex)
	{
		myLayerIndex = aLayerIndex;
	}

	int32 GetNodeIndex() const
	{
		return myNodeIndex;
	}

	void SetNodeIndex(const int32 aNodeIndex)
	{
		myNodeIndex = aNodeIndex;
	}

	uint8 GetSubnodeIndex() const
	{
		return mySubnodeIndex;
	}

	void SetSubnodeIndex(const uint8 aSubnodeIndex)
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

	bool operator==(const FSVONLink& aOther) const
	{
		return memcmp(this, &aOther, sizeof(FSVONLink)) == 0;
	}

	static FSVONLink GetInvalidLink()
	{
		return FSVONLink(15, 0, 0);
	}

	FString ToString()
	{
		return FString::Printf(TEXT("%i:%i:%i"), myLayerIndex, myNodeIndex, mySubnodeIndex);
	};
};

FORCEINLINE uint32 GetTypeHash(const FSVONLink& b)
{
	return HashCombine(HashCombine(GetTypeHash(b.myLayerIndex), GetTypeHash(b.myNodeIndex)), GetTypeHash(b.mySubnodeIndex));
}


FORCEINLINE FArchive& operator <<(FArchive& Ar, FSVONLink& aSVONLink)
{
	Ar.Serialize(&aSVONLink, sizeof(FSVONLink));
	return Ar;
}
