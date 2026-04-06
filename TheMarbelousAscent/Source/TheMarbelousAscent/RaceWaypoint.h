#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceWaypoint.generated.h"

UCLASS()
class THEMARBELOUSASCENT_API ARaceWaypoint : public AActor
{
	GENERATED_BODY()

public:
	ARaceWaypoint();

	// Index in the race path sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	int32 WaypointIndex;

	// Radius within which the waypoint is considered reached
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	float AcceptanceRadius;

	// Whether the AI should jump when approaching this waypoint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	bool bRequiresJump;

	// Target speed multiplier at this waypoint (0.0 - 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float SpeedMultiplier;

	// Whether the AI should brake approaching this waypoint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	bool bShouldBrake;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;
#endif

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* TriggerSphere;
};
