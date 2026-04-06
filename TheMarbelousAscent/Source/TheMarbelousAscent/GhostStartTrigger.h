#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GhostStartTrigger.generated.h"

class UAudioComponent;
class UBoxComponent;
class UStaticMeshComponent;
class AGhostRacerMarble;

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
// Also manages level BGM and player rolling/landing sounds.
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

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ButtonMesh;

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

	bool bPlayerWasInAir;

	void TickWaitingForTrigger(float DeltaTime);
	void TickCountdown(float DeltaTime);
	void UpdatePlayerAudio();
	void DimButton();
	UPrimitiveComponent* FindPlayerPhysicsComp(APawn* Pawn) const;
};
