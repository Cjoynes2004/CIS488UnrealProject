#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceResultWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

// Self-contained post-race results panel. Builds its own widget tree (a
// dimmed full-screen overlay with stacked text blocks) so no UMG editing is
// required.
UCLASS()
class THEMARBELOUSASCENT_API URaceResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URaceResultWidget(const FObjectInitializer& ObjectInitializer);

	// Populate the panel with the outcome.
	// bGhostFinished == false hides the ghost-time line entirely (it's
	// nonsense to show a partial playback time when the player won by
	// reaching the goal first).
	void ShowResult(bool bPlayerWon, float PlayerSeconds, float GhostSeconds, bool bGhostFinished);

protected:
	virtual bool Initialize() override;

private:
	UPROPERTY()
	UBorder* DimOverlay;

	UPROPERTY()
	UTextBlock* HeadlineText;

	UPROPERTY()
	UTextBlock* PlayerTimeText;

	UPROPERTY()
	UTextBlock* GhostTimeText;
};
