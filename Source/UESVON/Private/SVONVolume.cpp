#include "UESVON/Public/SVONVolume.h"
#include "UESVON.h"
#include "DrawDebugHelpers.h"
#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"

ASVONVolume::ASVONVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, myOrigin(FVector::ZeroVector)
	, myExtent(FVector::ZeroVector)
	, myDebugPosition(FVector::ZeroVector)
	, myIsReadyForNavigation(false)

{
	GetBrushComponent()->Mobility = EComponentMobility::Static;

	BrushColor = FColor(255, 255, 255, 255);

	bColored = true;

	FBox bounds = GetComponentsBoundingBox(true);
	bounds.GetCenterAndExtents(myOrigin, myExtent);
}

// Regenerates the Sparse Voxel Octree Navmesh
bool ASVONVolume::Generate()
{
#if WITH_EDITOR
	// Needed for debug rendering
	GetWorld()->PersistentLineBatcher->SetComponentTickEnabled(false);
	
	myDebugPosition = GetWorld()->ViewLocationsRenderedLastFrame[0];

	// If we're running the game, use the first player controller position for debugging
	APlayerController* pc = GetWorld()->GetFirstPlayerController();
	if (pc)
	{
		if(APawn* Pawn = pc->GetPawn())
		{
			myDebugPosition = Pawn->GetActorLocation();
		}
	}

	FlushPersistentDebugLines(GetWorld());

	// Setup timing
	double startTime = FPlatformTime::Seconds();

#endif // WITH_EDITOR

	UpdateBounds();

	// Clear data (for now)
	myBlockedIndices.Empty();
	myData.myLayers.Empty();

	myNumLayers = myVoxelPower + 1;

	// Rasterize at Layer 1
	FirstPassRasterize();

	// Allocate the leaf node data
	myData.myLeafNodes.Empty();
	myData.myLeafNodes.AddDefaulted(myBlockedIndices[0].Num() * 8 * 0.25f);

	// Add layers
	for (int i = 0; i < myNumLayers; i++)
	{
		myData.myLayers.Emplace();
	}

	// Rasterize layer, bottom up, adding parent/child links
	for (int i = 0; i < myNumLayers; i++)
	{
		RasterizeLayer(i);
	}

	// Now traverse down, adding neighbour links
	for (int i = myNumLayers - 2; i >= 0; i--)
	{
		BuildNeighbourLinks(i);
	}

#if WITH_EDITOR

	double endTime = FPlatformTime::Seconds();

	int32 totalNodes = 0;

	for (int i = 0; i < myNumLayers; i++)
	{
		totalNodes += myData.myLayers[i].Num();
	}

	int32 totalBytes = sizeof(FSVONNode) * totalNodes;
	totalBytes += sizeof(FSVONLeafNode) * myData.myLeafNodes.Num();

	UE_LOG(UESVON, Display, TEXT("Generation Time : %f"), endTime - startTime);
	UE_LOG(UESVON, Display, TEXT("Total Layers-Nodes : %d-%d"), myNumLayers, totalNodes);
	UE_LOG(UESVON, Display, TEXT("Total Leaf Nodes : %d"), myData.myLeafNodes.Num());
	UE_LOG(UESVON, Display, TEXT("Total Size (bytes): %d"), totalBytes);

#endif

	myNumBytes = myData.GetSize();

	return true;
}

void ASVONVolume::UpdateBounds()
{
	// Get bounds and extent
	FBox bounds = GetComponentsBoundingBox(true);
	bounds.GetCenterAndExtents(myOrigin, myExtent);
}

void ASVONVolume::ClearData()
{
	myData.Reset();
	myNumLayers = 0;
	myNumBytes = 0;
}

bool ASVONVolume::FirstPassRasterize()
{
	// Add the first layer of blocking
	myBlockedIndices.Emplace();

	int32 numNodes = GetNumNodesInLayer(1);
	for (int32 i = 0; i < numNodes; i++)
	{
		FVector position;
		GetNodePosition(1, i, position);
		FCollisionQueryParams params;
		params.bFindInitialOverlaps = true;
		params.bTraceComplex = false;
		params.TraceTag = "SVONFirstPassRasterize";
		if (GetWorld()->OverlapBlockingTestByChannel(position, FQuat::Identity, myCollisionChannel, FCollisionShape::MakeBox(FVector(GetVoxelSize(1) * 0.5f)), params))
		{
			myBlockedIndices[0].Add(i);
		}
	}

	int layerIndex = 0;

	while (myBlockedIndices[layerIndex].Num() > 1)
	{
		// Add a new layer to structure
		myBlockedIndices.Emplace();
		// Add any parent morton codes to the new layer
		for (uint64& code : myBlockedIndices[layerIndex])
		{
			myBlockedIndices[layerIndex + 1].Add(code >> 3);
		}
		layerIndex++;
	}

	return true;
}

bool ASVONVolume::GetNodePosition(uint8 aLayer, uint64 aCode, FVector& oPosition) const
{
	const float voxelSize = GetVoxelSize(aLayer);
	uint_fast32_t x, y, z;
	libmorton::morton3D_64_decode(aCode, x, y, z);
	oPosition = myOrigin - myExtent + FVector(x * voxelSize, y * voxelSize, z * voxelSize) + FVector(voxelSize * 0.5f);
	return true;
}

// Gets the position of a given link. Returns true if the link is open, false if blocked
bool ASVONVolume::GetLinkPosition(const FSVONLink& aLink, FVector& oPosition) const
{
	const FSVONNode& node = GetLayer(aLink.GetLayerIndex())[aLink.GetNodeIndex()];

	GetNodePosition(aLink.GetLayerIndex(), node.myCode, oPosition);
	// If this is layer 0, and there are valid children
	if (aLink.GetLayerIndex() == 0 && node.myFirstChild.IsValid())
	{
		float voxelSize = GetVoxelSize(0);
		uint_fast32_t x, y, z;
		libmorton::morton3D_64_decode(aLink.GetSubnodeIndex(), x, y, z);
		oPosition += FVector(x * voxelSize * 0.25f, y * voxelSize * 0.25f, z * voxelSize * 0.25f) - FVector(voxelSize * 0.375);
		const FSVONLeafNode& leafNode = GetLeafNode(node.myFirstChild.myNodeIndex);
		bool isBlocked = leafNode.GetNode(aLink.GetSubnodeIndex());
		return !isBlocked;
	}
	return true;
}

bool ASVONVolume::GetIndexForCode(uint8 aLayer, uint64 aCode, int32& oIndex) const
{
	const TArray<FSVONNode>& layer = GetLayer(aLayer);

	for (int i = 0; i < layer.Num(); i++)
	{
		if (layer[i].myCode == aCode)
		{
			oIndex = i;
			return true;
		}
	}

	return false;
}

const FSVONNode& ASVONVolume::GetNode(const FSVONLink& aLink) const
{
	if (aLink.GetLayerIndex() < 14)
	{
		return GetLayer(aLink.GetLayerIndex())[aLink.GetNodeIndex()];
	}
	else
	{
		return GetLayer(myNumLayers - 1)[0];
	}
}

const FSVONLeafNode& ASVONVolume::GetLeafNode(int32 aIndex) const
{
	return myData.myLeafNodes[aIndex];
}

void ASVONVolume::GetLeafNeighbours(const FSVONLink& aLink, TArray<FSVONLink>& oNeighbours) const
{
	uint64 leafIndex = aLink.GetSubnodeIndex();
	const FSVONNode& node = GetNode(aLink);
	const FSVONLeafNode& leaf = GetLeafNode(node.myFirstChild.GetNodeIndex());

	// Get our starting co-ordinates
	uint_fast32_t x = 0, y = 0, z = 0;
	libmorton::morton3D_64_decode(leafIndex, x, y, z);

	for (int i = 0; i < 6; i++)
	{
		// Need to switch to signed ints
		int32 sX = x + USVONStatics::dirs[i].X;
		int32 sY = y + USVONStatics::dirs[i].Y;
		int32 sZ = z + USVONStatics::dirs[i].Z;

		// If the neighbour is in bounds of this leaf node
		if (sX >= 0 && sX < 4 && sY >= 0 && sY < 4 && sZ >= 0 && sZ < 4)
		{
			uint64 thisIndex = libmorton::morton3D_64_encode(sX, sY, sZ);
			// If this node is blocked, then no link in this direction, continue
			if (leaf.GetNode(thisIndex))
			{
				continue;
			}
			else // Otherwise, this is a valid link, add it
			{
				oNeighbours.Emplace(0, aLink.GetNodeIndex(), thisIndex);
				continue;
			}
		}
		else // the neighbours is out of bounds, we need to find our neighbour
		{
			const FSVONLink& neighbourLink = node.myNeighbours[i];
			const FSVONNode& neighbourNode = GetNode(neighbourLink);

			// If the neighbour layer 0 has no leaf nodes, just return it
			if (!neighbourNode.myFirstChild.IsValid())
			{
				oNeighbours.Add(neighbourLink);
				continue;
			}

			const FSVONLeafNode& leafNode = GetLeafNode(neighbourNode.myFirstChild.GetNodeIndex());

			if (leafNode.IsCompletelyBlocked())
			{
				// The leaf node is completely blocked, we don't return it
				continue;
			}
			else // Otherwise, we need to find the correct subnode
			{
				if (sX < 0)
					sX = 3;
				else if (sX > 3)
					sX = 0;
				else if (sY < 0)
					sY = 3;
				else if (sY > 3)
					sY = 0;
				else if (sZ < 0)
					sZ = 3;
				else if (sZ > 3)
					sZ = 0;
				//
				uint64 subNodeCode = libmorton::morton3D_64_encode(sX, sY, sZ);

				// Only return the neighbour if it isn't blocked!
				if (!leafNode.GetNode(subNodeCode))
				{
					oNeighbours.Emplace(0, neighbourNode.myFirstChild.GetNodeIndex(), subNodeCode);
				}
			}
		}
	}
}

void ASVONVolume::GetNeighbours(const FSVONLink& aLink, TArray<FSVONLink>& oNeighbours) const
{
	const FSVONNode& node = GetNode(aLink);

	for (int i = 0; i < 6; i++)
	{
		const FSVONLink& neighbourLink = node.myNeighbours[i];

		if (!neighbourLink.IsValid())
			continue;

		const FSVONNode& neighbour = GetNode(neighbourLink);

		// If the neighbour has no children, it's empty, we just use it
		if (!neighbour.HasChildren())
		{
			oNeighbours.Add(neighbourLink);
			continue;
		}

		// If the node has children, we need to look down the tree to see which children we want to add to the neighbour set

		// Start working set, and put the link into it
		TArray<FSVONLink> workingSet;
		workingSet.Push(neighbourLink);

		while (workingSet.Num() > 0)
		{
			// Pop off the top of the working set
			FSVONLink thisLink = workingSet.Pop();
			const FSVONNode& thisNode = GetNode(thisLink);

			// If the node as no children, it's clear, so add to neighbours and continue
			if (!thisNode.HasChildren())
			{
				oNeighbours.Add(neighbourLink);
				continue;
			}

			// We know it has children

			if (thisLink.GetLayerIndex() > 0)
			{
				// If it's above layer 0, we will need to potentially add 4 children using our offsets
				for (const int32& childIndex : USVONStatics::dirChildOffsets[i])
				{
					// Each of the childnodes
					FSVONLink childLink = thisNode.myFirstChild;
					childLink.myNodeIndex += childIndex;
					const FSVONNode& childNode = GetNode(childLink);

					if (childNode.HasChildren()) // If it has children, add them to the working set to keep going down
					{
						workingSet.Emplace(childLink);
					}
					else // Or just add to the outgoing links
					{
						oNeighbours.Emplace(childLink);
					}
				}
			}
			else
			{
				// If this is a leaf layer, then we need to add whichever of the 16 facing leaf nodes aren't blocked
				for (const int32& leafIndex : USVONStatics::dirLeafChildOffsets[i])
				{
					// Each of the childnodes
					FSVONLink link = neighbour.myFirstChild;
					const FSVONLeafNode& leafNode = GetLeafNode(link.myNodeIndex);
					link.mySubnodeIndex = leafIndex;

					if (!leafNode.GetNode(leafIndex))
					{
						oNeighbours.Emplace(link);
					}
				}
			}
		}
	}
}

void ASVONVolume::Serialize(FArchive& Ar)
{
	// Serialize the usual UPROPERTIES
	Super::Serialize(Ar);

	if (myGenerationStrategy == ESVOGenerationStrategy::UseBaked)
	{
		Ar << myData;

		myNumLayers = myData.myLayers.Num();
		myNumBytes = myData.GetSize();
	}
}

float ASVONVolume::GetVoxelSize(uint8 aLayer) const
{
	return (myExtent.X / FMath::Pow(2, myVoxelPower)) * (FMath::Pow(2.0f, aLayer + 1));
}

bool ASVONVolume::IsReadyForNavigation() const
{
	return myIsReadyForNavigation;
}

int32 ASVONVolume::GetNumNodesInLayer(uint8 aLayer) const
{
	return FMath::Pow(FMath::Pow(2, (myVoxelPower - (aLayer))), 3);
}

int32 ASVONVolume::GetNumNodesPerSide(uint8 aLayer) const
{
	return FMath::Pow(2, (myVoxelPower - (aLayer)));
}

void ASVONVolume::BeginPlay()
{
	if (!myIsReadyForNavigation && myGenerationStrategy == ESVOGenerationStrategy::GenerateOnBeginPlay)
	{
		Generate();
	}
	else
	{
		UpdateBounds();
	}

	myIsReadyForNavigation = true;
}

void ASVONVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
}

void ASVONVolume::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();
}

void ASVONVolume::BuildNeighbourLinks(uint8 aLayer)
{

	TArray<FSVONNode>& layer = GetLayer(aLayer);
	uint8 searchLayer = aLayer;

	// For each node
	for (int32 i = 0; i < layer.Num(); i++)
	{
		FSVONNode& node = layer[i];
		// Get our world co-ordinate
		uint_fast32_t x, y, z;
		libmorton::morton3D_64_decode(node.myCode, x, y, z);
		int32 backtrackIndex = -1;
		int32 index = i;
		FVector nodePos;
		GetNodePosition(aLayer, node.myCode, nodePos);

		// For each direction
		for (int d = 0; d < 6; d++)
		{
			FSVONLink& linkToUpdate = node.myNeighbours[d];

			backtrackIndex = index;

			while (!FindLinkInDirection(searchLayer, index, d, linkToUpdate, nodePos) && aLayer < myData.myLayers.Num() - 2)
			{
				FSVONLink& parent = GetLayer(searchLayer)[index].myParent;
				if (parent.IsValid())
				{
					index = parent.myNodeIndex;
					searchLayer = parent.myLayerIndex;
				}
				else
				{
					searchLayer++;
					GetIndexForCode(searchLayer, node.myCode >> 3, index);
				}
			}
			index = backtrackIndex;
			searchLayer = aLayer;
		}
	}
}

bool ASVONVolume::FindLinkInDirection(uint8 aLayer, const int32 aNodeIndex, uint8 aDir, FSVONLink& oLinkToUpdate, FVector& aStartPosForDebug)
{
	int32 maxCoord = GetNumNodesPerSide(aLayer);
	FSVONNode& node = GetLayer(aLayer)[aNodeIndex];
	TArray<FSVONNode>& layer = GetLayer(aLayer);

	// Get our world co-ordinate
	uint_fast32_t x = 0, y = 0, z = 0;
	libmorton::morton3D_64_decode(node.myCode, x, y, z);
	int32 sX = x, sY = y, sZ = z;
	// Add the direction
	sX += USVONStatics::dirs[aDir].X;
	sY += USVONStatics::dirs[aDir].Y;
	sZ += USVONStatics::dirs[aDir].Z;

	// If the coords are out of bounds, the link is invalid.
	if (sX < 0 || sX >= maxCoord || sY < 0 || sY >= maxCoord || sZ < 0 || sZ >= maxCoord)
	{
		oLinkToUpdate.SetInvalid();
		if (myShowNeighbourLinks && IsInDebugRange(aStartPosForDebug))
		{
			FVector startPos, endPos;
			GetNodePosition(aLayer, node.myCode, startPos);
			endPos = startPos + (FVector(USVONStatics::dirs[aDir]) * 100.f);
			DrawDebugLine(GetWorld(), aStartPosForDebug, endPos, FColor::Red, true, -1.f, 0, .0f);
		}
		return true;
	}
	x = sX;
	y = sY;
	z = sZ;
	// Get the morton code for the direction
	uint64 thisCode = libmorton::morton3D_64_encode(x, y, z);
	bool isHigher = thisCode > node.myCode;
	int32 nodeDelta = (isHigher ? 1 : -1);

	while ((aNodeIndex + nodeDelta) < layer.Num() && aNodeIndex + nodeDelta >= 0)
	{
		// This is the node we're looking for
		if (layer[aNodeIndex + nodeDelta].myCode == thisCode)
		{
			const FSVONNode& thisNode = layer[aNodeIndex + nodeDelta];
			// This is a leaf node
			if (aLayer == 0 && thisNode.HasChildren())
			{
				// Set invalid link if the leaf node is completely blocked, no point linking to it
				if (GetLeafNode(thisNode.myFirstChild.GetNodeIndex()).IsCompletelyBlocked())
				{
					oLinkToUpdate.SetInvalid();
					return true;
				}
			}
			// Otherwise, use this link
			oLinkToUpdate.myLayerIndex = aLayer;
			check(aNodeIndex + nodeDelta < layer.Num());
			oLinkToUpdate.myNodeIndex = aNodeIndex + nodeDelta;
			if (myShowNeighbourLinks && IsInDebugRange(aStartPosForDebug))
			{
				FVector endPos;
				GetNodePosition(aLayer, thisCode, endPos);
				DrawDebugLine(GetWorld(), aStartPosForDebug, endPos, USVONStatics::myLinkColors[aLayer], true, -1.f, 0, .0f);
			}
			return true;
		}
		// If we've passed the code we're looking for, it's not on this layer
		else if ((isHigher && layer[aNodeIndex + nodeDelta].myCode > thisCode) || (!isHigher && layer[aNodeIndex + nodeDelta].myCode < thisCode))
		{
			return false;
		}

		nodeDelta += (isHigher ? 1 : -1);
	}

	// I'm not entirely sure if it's valid to reach the end? Hmmm...
	return false;
}

void ASVONVolume::RasterizeLeafNode(FVector& aOrigin, int32 aLeafIndex)
{
	for (int i = 0; i < 64; i++)
	{

		uint_fast32_t x, y, z;
		libmorton::morton3D_64_decode(i, x, y, z);
		float leafVoxelSize = GetVoxelSize(0) * 0.25f;
		FVector position = aOrigin + FVector(x * leafVoxelSize, y * leafVoxelSize, z * leafVoxelSize) + FVector(leafVoxelSize * 0.5f);

		if (aLeafIndex >= myData.myLeafNodes.Num() - 1)
			myData.myLeafNodes.AddDefaulted(1);

		if (IsBlocked(position, leafVoxelSize * 0.5f))
		{
			myData.myLeafNodes[aLeafIndex].SetNode(i);

			if (myShowLeafVoxels && IsInDebugRange(position))
			{
				DrawDebugBox(GetWorld(), position, FVector(leafVoxelSize * 0.5f), FQuat::Identity, FColor::Red, true, -1.f, 0, .0f);
			}
			if (myShowMortonCodes && IsInDebugRange(position))
			{
				DrawDebugString(GetWorld(), position, FString::FromInt(aLeafIndex) + ":" + FString::FromInt(i), nullptr, FColor::Red, -1, false);
			}
		}
	}
}

// Check for blocking...using this cached set for each layer for now for fast lookups
bool ASVONVolume::IsAnyMemberBlocked(uint8 aLayer, uint64 aCode) const
{
	uint64 parentCode = aCode >> 3;

	if (aLayer == myBlockedIndices.Num())
	{
		return true;
	}
	// The parent of this code is blocked
	if (myBlockedIndices[aLayer].Contains(parentCode))
	{
		return true;
	}

	return false;
}

// World blocking test here, we're using a physics box trace at the moment
bool ASVONVolume::IsBlocked(const FVector& aPosition, const float aSize) const
{
	FCollisionQueryParams params;
	params.bFindInitialOverlaps = true;
	params.bTraceComplex = false;
	params.TraceTag = "SVONLeafRasterize";

	return GetWorld()->OverlapBlockingTestByChannel(aPosition, FQuat::Identity, myCollisionChannel, FCollisionShape::MakeBox(FVector(aSize + myClearance)), params);
}

bool ASVONVolume::IsInDebugRange(const FVector& aPosition) const
{
	return FVector::DistSquared(myDebugPosition, aPosition) < myDebugDistance * myDebugDistance;
}

void ASVONVolume::RasterizeLayer(uint8 aLayer)
{
	int32 leafIndex = 0;
	// Layer 0 Leaf nodes are special
	if (aLayer == 0)
	{
		// Run through all our coordinates
		int32 numNodes = GetNumNodesInLayer(aLayer);
		for (int32 i = 0; i < numNodes; i++)
		{
			int index = (i);

			// If we know this node needs to be added, from the low res first pass
			if (myBlockedIndices[0].Contains(i >> 3))
			{
				// Add a node
				index = GetLayer(aLayer).Emplace();
				FSVONNode& node = GetLayer(aLayer)[index];

				// Set my code and position
				node.myCode = (i);

				FVector nodePos;
				GetNodePosition(aLayer, node.myCode, nodePos);

				// Debug stuff
				if (myShowMortonCodes && IsInDebugRange(nodePos))
				{
					DrawDebugString(GetWorld(), nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), nullptr, USVONStatics::myLayerColors[aLayer], -1, false);
				}
				if (myShowVoxels && IsInDebugRange(nodePos))
				{
					DrawDebugBox(GetWorld(), nodePos, FVector(GetVoxelSize(aLayer) * 0.5f), FQuat::Identity, USVONStatics::myLayerColors[aLayer], true, -1.f, 0, .0f);
				}

				// Now check if we have any blocking, and search leaf nodes
				FVector position;
				GetNodePosition(0, i, position);

				FCollisionQueryParams params;
				params.bFindInitialOverlaps = true;
				params.bTraceComplex = false;
				params.TraceTag = "SVONRasterize";

				if (IsBlocked(position, GetVoxelSize(0) * 0.5f))
				{
					// Rasterize my leaf nodes
					FVector leafOrigin = nodePos - (FVector(GetVoxelSize(aLayer) * 0.5f));
					RasterizeLeafNode(leafOrigin, leafIndex);
					node.myFirstChild.SetLayerIndex(0);
					node.myFirstChild.SetNodeIndex(leafIndex);
					node.myFirstChild.SetSubnodeIndex(0);
					leafIndex++;
				}
				else
				{
					myData.myLeafNodes.AddDefaulted(1);
					leafIndex++;
					node.myFirstChild.SetInvalid();
				}
			}
		}
	}
	// Deal with the other layers
	else if (GetLayer(aLayer - 1).Num() > 1)
	{
		int nodeCounter = 0;
		int32 numNodes = GetNumNodesInLayer(aLayer);
		for (int32 i = 0; i < numNodes; i++)
		{
			// Do we have any blocking children, or siblings?
			// Remember we must have 8 children per parent
			if (IsAnyMemberBlocked(aLayer, i))
			{
				// Add a node
				int32 index = GetLayer(aLayer).Emplace();
				nodeCounter++;
				FSVONNode& node = GetLayer(aLayer)[index];
				// Set details
				node.myCode = i;
				int32 childIndex = 0;
				if (GetIndexForCode(aLayer - 1, node.myCode << 3, childIndex))
				{
					// Set parent->child links
					node.myFirstChild.SetLayerIndex(aLayer - 1);
					node.myFirstChild.SetNodeIndex(childIndex);
					// Set child->parent links, this can probably be done smarter, as we're duplicating work here
					for (int iter = 0; iter < 8; iter++)
					{
						GetLayer(node.myFirstChild.GetLayerIndex())[node.myFirstChild.GetNodeIndex() + iter].myParent.SetLayerIndex(aLayer);
						GetLayer(node.myFirstChild.GetLayerIndex())[node.myFirstChild.GetNodeIndex() + iter].myParent.SetNodeIndex(index);
					}

					if (myShowParentChildLinks) // Debug all the things
					{
						FVector startPos, endPos;
						GetNodePosition(aLayer, node.myCode, startPos);
						GetNodePosition(aLayer - 1, node.myCode << 3, endPos);
						if (IsInDebugRange(startPos))
							DrawDebugDirectionalArrow(GetWorld(), startPos, endPos, 0.f, USVONStatics::myLinkColors[aLayer], true);
					}
				}
				else
				{
					node.myFirstChild.SetInvalid();
				}

				if (myShowMortonCodes || myShowVoxels)
				{
					FVector nodePos;
					GetNodePosition(aLayer, i, nodePos);

					// Debug stuff
					if (myShowVoxels && IsInDebugRange(nodePos))
					{
						DrawDebugBox(GetWorld(), nodePos, FVector(GetVoxelSize(aLayer) * 0.5f), FQuat::Identity, USVONStatics::myLayerColors[aLayer], true, -1.f, 0, .0f);
					}
					if (myShowMortonCodes && IsInDebugRange(nodePos))
					{
						DrawDebugString(GetWorld(), nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), nullptr, USVONStatics::myLayerColors[aLayer], -1, false);
					}
				}
			}
		}
	}
}
