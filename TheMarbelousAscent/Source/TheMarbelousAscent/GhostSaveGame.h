#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GhostTypes.h"
#include "GhostSaveGame.generated.h"

UCLASS()
class THEMARBELOUSASCENT_API UGhostSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// The best ghost run for each level (keyed by level name)
	UPROPERTY()
	TMap<FString, FGhostRun> BestRuns;

	// Save slot name used for all ghost data
	static FString GetSlotName() { return TEXT("GhostData"); }
};
