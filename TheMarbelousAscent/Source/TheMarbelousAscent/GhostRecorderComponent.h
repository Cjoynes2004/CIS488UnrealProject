#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GhostTypes.h"
#include "GhostRecorderComponent.generated.h"

UCLASS(ClassGroup=(Ghost), meta=(BlueprintSpawnableComponent))
class THEMARBELOUSASCENT_API UGhostRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGhostRecorderComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// How many samples per second on the ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float GroundSampleRate;

	// How many samples per second in the air (higher for accuracy)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	float AirSampleRate;

	// Start recording the player's run
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void StartRecording();

	// Stop recording and save if it's the best run
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void StopRecording();

	// Is currently recording?
	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool IsRecording() const { return bIsRecording; }

	// Get the current run data (for immediate playback without saving)
	const FGhostRun& GetCurrentRun() const { return CurrentRun; }

protected:
	virtual void BeginPlay() override;

private:
	bool bIsRecording;
	FGhostRun CurrentRun;
	float RecordTimer;
	FVector LastRecordedLocation;
	FRotator LastRecordedRotation;

	void RecordSample();
	bool ShouldForceSample() const;
	float GetCurrentSampleInterval() const;
	bool IsOwnerInAir() const;
	void SaveBestRun();
};
