#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GhostTypes.h"
#include "GhostRacerMarble.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// Plays back a recorded ghost run as a translucent, non-colliding marble.
// No physics, no AI — just interpolated transform playback.
UCLASS()
class THEMARBELOUSASCENT_API AGhostRacerMarble : public APawn
{
	GENERATED_BODY()

public:
	AGhostRacerMarble();

	virtual void Tick(float DeltaTime) override;

	// Appearance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float GhostOpacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	FLinearColor GhostColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float GlowIntensity;

	// Load and start playing back a ghost run
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void StartPlayback(const FGhostRun& Run);

	// Stop playback
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void StopPlayback();

	// Is currently playing?
	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool IsPlaying() const { return bIsPlaying; }

	// Load the best saved ghost for the current level and start playback
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void LoadAndPlay();

	// Get how far along the ghost is (0 to 1)
	UFUNCTION(BlueprintPure, Category = "Ghost")
	float GetProgress() const;

	// Get the ghost's current elapsed time
	UFUNCTION(BlueprintPure, Category = "Ghost")
	float GetElapsedTime() const { return PlaybackTime; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	USphereComponent* SphereCollision;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MarbleMesh;

	FGhostRun GhostRun;
	bool bIsPlaying;
	float PlaybackTime;
	int32 CurrentSampleIndex;
	bool bGhostDataReady;

	void UpdatePlayback(float DeltaTime);
	void ApplyGhostAppearance();
	int32 FindSampleIndex(float Time) const;
	FVector HermiteInterpolateLocation(const FGhostSample& A, const FGhostSample& B, float Alpha) const;
	FGhostRun GenerateStaffGhost() const;
};
