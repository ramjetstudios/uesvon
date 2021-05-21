#pragma once

#include "UESVON/Public/SVONData.h"
#include "UESVON/Public/SVONDefines.h"
#include "UESVON/Public/SVONLeafNode.h"
#include "UESVON/Public/SVONNode.h"
#include "GameFramework/Volume.h"
#include "SVONVolume.generated.h"

UENUM(BlueprintType)
enum class ESVOGenerationStrategy : uint8
{
	UseBaked UMETA(DisplayName = "Use Baked"),
	GenerateOnBeginPlay UMETA(DisplayName = "Generate OnBeginPlay")
};

/**
 *  SVONVolume contains the navigation data for the volume, and the methods for generating that data
		See SVONMediator for public query functions
 */
UCLASS(hidecategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class UESVON_API ASVONVolume : public AVolume
{
	GENERATED_BODY()

public:
	ASVONVolume(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	void BeginPlay() override;
	void PostRegisterAllComponents() override;
	void PostUnregisterAllComponents() override;

	bool ShouldTickIfViewportsOnly() const override
	{
		return true;
	}

	//~ End AActor Interface

	//~ Begin UObject 
	void Serialize(FArchive& Ar) override;
	//~ End UObject 

	bool Generate();
	void ClearData();

	bool IsReadyForNavigation() const;

	const TArray<FSVONNode>& GetLayer(uint8 aLayer) const
	{
		return myData.myLayers[aLayer];
	};
	const FSVONNode& GetNode(const FSVONLink& aLink) const;
	const FSVONLeafNode& GetLeafNode(int32 aIndex) const;
	bool GetLinkPosition(const FSVONLink& aLink, FVector& oPosition) const;
	bool GetNodePosition(uint8 aLayer, uint64 aCode, FVector& oPosition) const;
	void GetLeafNeighbours(const FSVONLink& aLink, TArray<FSVONLink>& oNeighbours) const;
	void GetNeighbours(const FSVONLink& aLink, TArray<FSVONLink>& oNeighbours) const;
	float GetVoxelSize(uint8 aLayer) const;

	const uint8 GetMyNumLayers() const
	{
		return myNumLayers;
	}

	// Debug Info
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	float myDebugDistance = 5000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	bool myShowVoxels = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	bool myShowLeafVoxels = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	bool myShowMortonCodes = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	bool myShowNeighbourLinks = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	bool myShowParentChildLinks = false;

	// Generation parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	int32 myVoxelPower = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	TEnumAsByte<ECollisionChannel> myCollisionChannel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	float myClearance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UESVON")
	ESVOGenerationStrategy myGenerationStrategy = ESVOGenerationStrategy::UseBaked;

	// Generated data attributes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESVON")
	uint8 myNumLayers = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UESVON")
	int myNumBytes = 0;

private:
	// The navigation data
	FSVONData myData;
	// temporary data used during nav data generation first pass rasterize
	TArray<TSet<uint64>> myBlockedIndices;
	// Helper members
	FVector myOrigin;
	FVector myExtent;
	// Used for defining debug visualiation range
	FVector myDebugPosition;

	TArray<FSVONNode>& GetLayer(uint8 aLayer)
	{
		return myData.myLayers[aLayer];
	};

	bool myIsReadyForNavigation;

	void UpdateBounds();

	// Generation methods
	bool FirstPassRasterize();
	void RasterizeLayer(uint8 aLayer);
	void BuildNeighbourLinks(uint8 aLayer);
	bool FindLinkInDirection(uint8 aLayer, const int32 aNodeIndex, uint8 aDir, FSVONLink& oLinkToUpdate, FVector& aStartPosForDebug);
	void RasterizeLeafNode(FVector& aOrigin, int32 aLeafIndex);

	bool GetIndexForCode(uint8 aLayer, uint64 aCode, int32& oIndex) const;
	bool IsAnyMemberBlocked(uint8 aLayer, uint64 aCode) const;
	bool IsBlocked(const FVector& aPosition, const float aSize) const;
	int32 GetNumNodesInLayer(uint8 aLayer) const;
	int32 GetNumNodesPerSide(uint8 aLayer) const;

	bool IsInDebugRange(const FVector& aPosition) const;
};
