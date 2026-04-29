#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelFadeIn.generated.h"

class UUserWidget;

// Spawns the WBP_Fade widget on world begin play so every level starts with a
// black-to-clear fade-in. Looks for the widget at /Game/UI/Widgets/WBP_Fade
// and silently no-ops if it isn't found, so this stays safe even if WBP_Fade
// gets renamed or removed.
UCLASS()
class THEMARBELOUSASCENT_API ULevelFadeIn : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
