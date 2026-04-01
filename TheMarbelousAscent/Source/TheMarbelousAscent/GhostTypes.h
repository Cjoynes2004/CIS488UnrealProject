#pragma once

#include "CoreMinimal.h"
#include "GhostTypes.generated.h"

// A single snapshot of the marble's state at a point in time
USTRUCT(BlueprintType)
struct FGhostSample
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector Velocity;

	FGhostSample()
		: Time(0.0f)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Velocity(FVector::ZeroVector)
	{
	}
};

// A complete recorded run
USTRUCT(BlueprintType)
struct FGhostRun
{
	GENERATED_BODY()

	UPROPERTY()
	FString LevelName;

	UPROPERTY()
	float FinalTime;

	UPROPERTY()
	TArray<FGhostSample> Samples;

	FGhostRun()
		: FinalTime(0.0f)
	{
	}

	bool IsValid() const
	{
		return Samples.Num() > 1;
	}
};
