#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceCountdownWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

// Self-contained countdown HUD. Builds its own widget tree (centered image +
// fallback text) so no UMG editing is required. Drops PNG textures imported
// to /Game/UI/Textures/Countdown/{Three,Two,One,Go} (any of the supported UE
// texture naming variants — see code) and the widget displays them; if the
// textures aren't present it falls back to displaying the digit as text.
UCLASS()
class THEMARBELOUSASCENT_API URaceCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URaceCountdownWidget(const FObjectInitializer& ObjectInitializer);

	// Show the digit for the current second (3, 2, 1).
	void ShowNumber(int32 Number);

	// Show the "Go!" frame at race start.
	void ShowGo();

protected:
	virtual bool Initialize() override;

private:
	UPROPERTY()
	UImage* DisplayImage;

	UPROPERTY()
	UImage* OutlineImage;

	UPROPERTY()
	UTextBlock* FallbackText;

	UPROPERTY()
	TArray<UTexture2D*> NumberTextures;

	UPROPERTY()
	UTexture2D* GoTexture;

	void SetTextureOrText(UTexture2D* Texture, const FString& Fallback);
};
