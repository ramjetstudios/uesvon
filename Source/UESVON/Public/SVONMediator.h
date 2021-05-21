#pragma once
#include "SVONMediator.generated.h"

struct FSVONLink;

UCLASS(BlueprintType)
class UESVON_API USVONMediator : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SVON")
	static bool GetLinkFromPosition(const FVector& aPosition, const class ASVONVolume* aVolume, FSVONLink& oLink);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SVON")
	static void GetVolumeXYZ(const FVector& aPosition, const class ASVONVolume* aVolume, const int aLayer, FIntVector& oXYZ);
};