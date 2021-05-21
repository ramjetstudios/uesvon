#pragma once
#include "AITypes.h"
#include "SVONDefines.generated.h"

UENUM(BlueprintType)
enum class EBuildTrigger : uint8
{
	OnEdit UMETA(DisplayName = "On Edit"),
	Manual UMETA(DisplayName = "Manual")
};

#define LEAF_LAYER_INDEX 14;

UCLASS(BlueprintType)
class UESVON_API USVONStatics : public UObject
{
	GENERATED_BODY()
	
public:
	static const FIntVector dirs[];
	static const int32 dirChildOffsets[6][4];
	static const int32 dirLeafChildOffsets[6][16];
	static const FColor myLayerColors[];
	static const FColor myLinkColors[];
};

UENUM(BlueprintType)
enum class ESVONPathfindingRequestResult : uint8
{
	Failed,
	// Something went wrong
	ReadyToPath,
	// Pre-reqs satisfied
	AlreadyAtGoal,
	// No need to move
	Deferred,
	// Passed request to another thread, need to wait
	Success // it worked!
};

USTRUCT(BlueprintType)
struct FSVONPathfindingRequestResult
{
	GENERATED_BODY()
	
	FAIRequestID MoveId = FAIRequestID::InvalidRequest;
	UPROPERTY(BlueprintReadOnly, Category="SVON")
	ESVONPathfindingRequestResult Code = ESVONPathfindingRequestResult::Failed;

	operator ESVONPathfindingRequestResult() const
	{
		return Code;
	}
};
