// Fill out your copyright notice in the Description page of Project Settings.

#include "UESVON/Public/SVONNavigationComponent.h"
#include "UESVON.h"
#include "DrawDebugHelpers.h"
#include "SVONMediator.h"
#include "Kismet/GameplayStatics.h"
#include "UESVON/Public/SVONFindPathTask.h"
#include "UESVON/Public/SVONLink.h"
#include "UESVON/Public/SVONNavigationPath.h"
#include "UESVON/Public/SVONPathFinder.h"
#include "UESVON/Public/SVONVolume.h"

// Sets default values for this component's properties
USVONNavigationComponent::USVONNavigationComponent(const FObjectInitializer& ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	LastLocation = FSVONLink(0, 0, 0);

	SVONPath = MakeShareable<FSVONNavigationPath>(new FSVONNavigationPath());
}

bool USVONNavigationComponent::HasNavData() const
{
	return CurrentNavVolume && GetOwner() && CurrentNavVolume->EncompassesPoint(GetPawnPosition()) && CurrentNavVolume->GetMyNumLayers() > 0;
}

bool USVONNavigationComponent::FindVolume()
{
	TArray<AActor*> navVolumes;

	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASVONVolume::StaticClass(), navVolumes);

	for (AActor* actor : navVolumes)
	{
		ASVONVolume* volume = Cast<ASVONVolume>(actor);
		if (volume && volume->EncompassesPoint(GetPawnPosition()))
		{
			CurrentNavVolume = volume;
			return true;
		}
	}
	return false;
}

// Called every frame
void USVONNavigationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//return;

	if (!HasNavData())
	{
		FindVolume();
	}
	else if (CurrentNavVolume->IsReadyForNavigation()) // && !myIsBusy)
	{
		FVector location = GetPawnPosition();
		if (DebugPrintMortonCodes)
		{
			DebugLocalPosition();
		}
		FSVONLink link = GetNavPosition(location);
	}
}

FSVONLink USVONNavigationComponent::GetNavPosition(FVector& aPosition) const
{
	FSVONLink navLink;
	if (HasNavData())
	{
		// Get the nav link from our volume
		USVONMediator::GetLinkFromPosition(GetOwner()->GetActorLocation(), CurrentNavVolume, navLink);

		if (navLink == LastLocation)
			return navLink;

		LastLocation = navLink;

#if WITH_EDITORONLY_DATA
		if (DebugPrintCurrentPosition)
		{
			const FSVONNode& currentNode = CurrentNavVolume->GetNode(navLink);
			FVector currentNodePosition;

			bool isValid = CurrentNavVolume->GetLinkPosition(navLink, currentNodePosition);

			DrawDebugLine(GetWorld(), GetPawnPosition(), currentNodePosition, isValid ? FColor::Green : FColor::Red, false, -1.f, 0, 10.f);
			DrawDebugString(GetWorld(), GetPawnPosition() + FVector(0.f, 0.f, -50.f), navLink.ToString(), NULL, FColor::Yellow, 0.01f);
		}
#endif
	}
	return navLink;
}

bool USVONNavigationComponent::FindPathAsync(const FVector& aStartPosition, const FVector& aTargetPosition, FThreadSafeBool& aCompleteFlag, FSVONNavPathSharedPtr* oNavPath)
{
#if WITH_EDITOR
	UE_LOG(UESVON, Log, TEXT("Finding path from %s and %s"), *aStartPosition.ToString(), *aTargetPosition.ToString());
#endif
	FSVONLink startNavLink;
	FSVONLink targetNavLink;
	if (HasNavData())
	{
		// Get the nav link from our volume
		if (!USVONMediator::GetLinkFromPosition(aStartPosition, CurrentNavVolume, startNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Error, TEXT("Path finder failed to find start nav link. Is your pawn blocking the channel you've selected to generate the nav data with?"));
#endif
			return false;
		}

		if (!USVONMediator::GetLinkFromPosition(aTargetPosition, CurrentNavVolume, targetNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Error, TEXT("Path finder failed to find target nav link"));
#endif
			return false;
		}

		FSVONPathFinderSettings settings;
		settings.myUseUnitCost = UseUnitCost;
		settings.myUnitCost = UnitCost;
		settings.myEstimateWeight = EstimateWeight;
		settings.myNodeSizeCompensation = NodeSizeCompensation;
		settings.myPathCostType = PathCostType;
		settings.mySmoothingIterations = SmoothingIterations;

		(new FAutoDeleteAsyncTask<FSVONFindPathTask>(CurrentNavVolume, settings, GetWorld(), startNavLink, targetNavLink, aStartPosition, aTargetPosition, oNavPath, aCompleteFlag))->StartBackgroundTask();

		return true;
	}
	else
	{
#if WITH_EDITOR
		UE_LOG(UESVON, Error, TEXT("Pawn is not inside an SVON volume, or nav data has not been generated"));
#endif
	}
	return false;
}

bool USVONNavigationComponent::FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, FSVONNavPathSharedPtr* oNavPath)
{
#if WITH_EDITOR
	UE_LOG(UESVON, Log, TEXT("Finding path immediate from %s and %s"), *aStartPosition.ToString(), *aTargetPosition.ToString());
#endif

	FSVONLink startNavLink;
	FSVONLink targetNavLink;
	if (HasNavData())
	{
		// Get the nav link from our volume
		if (!USVONMediator::GetLinkFromPosition(aStartPosition, CurrentNavVolume, startNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Error, TEXT("Path finder failed to find start nav link"));
#endif
			return false;
		}

		if (!USVONMediator::GetLinkFromPosition(aTargetPosition, CurrentNavVolume, targetNavLink))
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Error, TEXT("Path finder failed to find target nav link"));
#endif
			return false;
		}

		if (!oNavPath || !oNavPath->IsValid())
		{
#if WITH_EDITOR
			UE_LOG(UESVON, Error, TEXT("Nav path data invalid"));
#endif
			return false;
		}

		FSVONNavigationPath* path = oNavPath->Get();

		path->ResetForRepath();

		FSVONPathFinderSettings settings;
		settings.myUseUnitCost = UseUnitCost;
		settings.myUnitCost = UnitCost;
		settings.myEstimateWeight = EstimateWeight;
		settings.myNodeSizeCompensation = NodeSizeCompensation;
		settings.myPathCostType = PathCostType;
		settings.mySmoothingIterations = SmoothingIterations;

		FSVONPathFinder pathFinder(CurrentNavVolume, settings);

		int result = pathFinder.FindPath(startNavLink, targetNavLink, aStartPosition, aTargetPosition, oNavPath);

		path->SetIsReady(true);

		return true;
	}
	else
	{
#if WITH_EDITOR
		UE_LOG(UESVON, Error, TEXT("Pawn is not inside an SVON volume, or nav data has not been generated"));
#endif
	}

	return false;
}

void USVONNavigationComponent::FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, TArray<FVector>& OutPathPoints)
{
	FindPathImmediate(aStartPosition, aTargetPosition, &SVONPath);

	OutPathPoints.Empty();

	for (const FSVONPathPoint& PathPoint : SVONPath->GetPathPoints())
	{
		OutPathPoints.Emplace(PathPoint.myPosition);
	}
}

void USVONNavigationComponent::DebugLocalPosition()
{
	if (HasNavData())
	{
		for (int i = 0; i < CurrentNavVolume->GetMyNumLayers() - 1; i++)
		{
			FIntVector pos;
			USVONMediator::GetVolumeXYZ(GetPawnPosition(), CurrentNavVolume, i, pos);
			uint64 code = libmorton::morton3D_64_encode(pos.X, pos.Y, pos.Z);
			FString codeString = FString::FromInt(code);
			DrawDebugString(GetWorld(), GetPawnPosition() + FVector(0.f, 0.f, i * 50.0f), pos.ToString() + " - " + codeString, NULL, FColor::White, 0.01f);
		}
	}
}

FVector USVONNavigationComponent::GetPawnPosition() const
{
	FVector result;

	AController* controller = Cast<AController>(GetOwner());

	if (controller)
	{
		if (APawn* pawn = controller->GetPawn())
			result = pawn->GetActorLocation();
	}

	return result;
}
