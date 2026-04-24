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

	// Rolling animation — sphere angular speed = linear speed / radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Rolling")
	float MarbleRadius;

	// Rubber-banding — distance along ghost's forward direction at which speed starts scaling.
	// Positive forward distance = player ahead of ghost (ghost speeds up).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Rubber Band")
	float RubberBandDeadZone;

	// Over this extra distance, speed interpolates fully to min/max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Rubber Band")
	float RubberBandRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Rubber Band")
	float MinPlaybackSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Rubber Band")
	float MaxPlaybackSpeed;

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

	FQuat AccumulatedRoll;
	FVector LastFrameLocation;

	// Per-sample moving platform tracking. If SamplePlatforms[i] is non-null, that sample
	// was placed on a moving platform; at playback the sample is offset by the platform's
	// current world motion so the ghost rides the platform.
	UPROPERTY()
	TArray<AActor*> SamplePlatforms;

	// Offset from the platform's world origin to the ball's world position. Constant.
	TArray<FVector> SampleOffsets;

	FVector GetAdjustedSampleLocation(int32 SampleIdx) const;

	void UpdatePlayback(float DeltaTime);
	void ApplyGhostAppearance();
	int32 FindSampleIndex(float Time) const;
	FVector HermiteInterpolateLocation(const FGhostSample& A, const FGhostSample& B, float Alpha) const;
	FGhostRun GenerateStaffGhost() const;
};
