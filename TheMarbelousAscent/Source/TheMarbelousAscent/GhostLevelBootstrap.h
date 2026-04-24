#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GhostLevelBootstrap.generated.h"

// Spawns the ghost start trigger and ghost racer marble at runtime on levels
// that should have the ghost racer feature. Runtime spawn avoids needing to
// place the actors manually in every level uasset; if they are already placed
// in the level (via the editor) the bootstrap detects that and skips.
UCLASS()
class THEMARBELOUSASCENT_API UGhostLevelBootstrap : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
