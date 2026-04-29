#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GhostStartTrigger.generated.h"

class UAudioComponent;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class AGhostRacerMarble;
class URaceCountdownWidget;
class URaceResultWidget;

UENUM()
enum class ERaceState : uint8
{
	WaitingForTrigger,
	Countdown,
	Racing,
	Finished
};

// Place this near the start of the level. When the player marble rolls over it,
// both player and ghost are placed side-by-side, a countdown plays, then the race begins.
// Also detects the finish line and shows a result HUD.
UCLASS()
class THEMARBELOUSASCENT_API AGhostStartTrigger : public AActor
{
	GENERATED_BODY()

public:
	AGhostStartTrigger();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	AGhostRacerMarble* GhostMarble;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float TriggerRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	FLinearColor ButtonColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Race Start")
	FVector PlayerStartPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Race Start")
	FVector GhostStartPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Race Start")
	float CountdownDuration;

	// World-space center of the finish-line cube (defaults to Cube136). The
	// race ends when the player gets within FinishLineRadius of this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Finish Line")
	FVector FinishLineLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Finish Line")
	float FinishLineRadius;

	// Optional UI. If left null, the trigger falls back to on-screen debug text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|UI")
	TSubclassOf<URaceCountdownWidget> CountdownWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|UI")
	TSubclassOf<URaceResultWidget> ResultWidgetClass;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ButtonMesh;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* GlowLight;

	ERaceState RaceState;
	float CountdownTimer;
	int32 LastDisplayedNumber;

	// Cached during trigger
	APawn* CachedPlayerPawn;
	UPrimitiveComponent* CachedPlayerPhysicsComp;

	// Sounds
	USoundWave* CountdownTickSound;
	USoundWave* CountdownGoSound;
	USoundWave* RollingSound;
	USoundWave* LandingSound;

	UPROPERTY()
	UAudioComponent* CountdownAudioComp;

	UPROPERTY()
	UAudioComponent* RollingAudioComp;

	UPROPERTY()
	UAudioComponent* BGMAudioComp;

	UPROPERTY()
	URaceCountdownWidget* CountdownWidget;

	UPROPERTY()
	URaceResultWidget* ResultWidget;

	bool bPlayerWasInAir;

	// Race timing
	float RaceStartGameTime;

	// Cached emissive intensity baseline for button pulsing
	float ButtonPulseTime;

	// Debug screenshot capture state
	float DebugScreenshotTimer;
	int32 DebugScreenshotCount;

	void TickWaitingForTrigger(float DeltaTime);
	void TickCountdown(float DeltaTime);
	void TickRacing(float DeltaTime);
	void UpdatePlayerAudio();
	void UpdateButtonPulse(float DeltaTime);
	void DimButton();
	void FinishRace(bool bPlayerWon);
	UPrimitiveComponent* FindPlayerPhysicsComp(APawn* Pawn) const;
};
