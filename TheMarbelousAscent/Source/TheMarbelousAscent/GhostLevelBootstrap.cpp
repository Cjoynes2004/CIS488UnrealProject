#include "GhostLevelBootstrap.h"
#include "GhostStartTrigger.h"
#include "GhostRacerMarble.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"

bool UGhostLevelBootstrap::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only tick in game-style worlds (PIE, standalone). No editor preview, no inactive.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UGhostLevelBootstrap::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Only auto-spawn for TutorialTest. Extend here if more levels get the feature.
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(&InWorld);
	if (LevelName != TEXT("TutorialTest"))
	{
		return;
	}

	// If a designer has placed the actors in the level already, respect that and bail.
	TArray<AActor*> ExistingTriggers;
	UGameplayStatics::GetAllActorsOfClass(&InWorld, AGhostStartTrigger::StaticClass(), ExistingTriggers);
	TArray<AActor*> ExistingGhosts;
	UGameplayStatics::GetAllActorsOfClass(&InWorld, AGhostRacerMarble::StaticClass(), ExistingGhosts);
	if (ExistingTriggers.Num() > 0 || ExistingGhosts.Num() > 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The marble is spawned first so the trigger's BeginPlay auto-binding finds it.
	// Its initial position doesn't matter (trigger teleports it on activation).
	InWorld.SpawnActor<AGhostRacerMarble>(
		AGhostRacerMarble::StaticClass(),
		FVector(-780.0f, 160.0f, 400.0f),
		FRotator::ZeroRotator,
		Params);

	// Place the trigger on the ground directly under the "Cube137" signpost.
	// X/Y match the sign; Z is found by tracing straight down from just below
	// the sign's bounding box so we don't accidentally hit the sign itself.
	const FVector SignXY(-943.0f, 835.0f, 0.0f);
	FVector TriggerLoc = SignXY;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GhostTriggerGroundTrace), false);
	// Sign's bbox bottom is around Z=200 (center 400, half-height 200).
	// Start the trace from Z=150 so we're already below the sign.
	const FVector TraceStart = SignXY + FVector(0.0f, 0.0f, 150.0f);
	const FVector TraceEnd = SignXY + FVector(0.0f, 0.0f, -5000.0f);
	if (InWorld.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Query))
	{
		// Cylinder mesh is pivoted at its base, so spawning at the floor Z
		// plants the button flush on the ground.
		TriggerLoc = Hit.Location;
	}

	UE_LOG(LogTemp, Log, TEXT("GhostLevelBootstrap: trigger ground trace -> (%.0f, %.0f, %.0f)"),
		TriggerLoc.X, TriggerLoc.Y, TriggerLoc.Z);
	InWorld.SpawnActor<AGhostStartTrigger>(
		AGhostStartTrigger::StaticClass(),
		TriggerLoc,
		FRotator::ZeroRotator,
		Params);

	UE_LOG(LogTemp, Log, TEXT("GhostLevelBootstrap: spawned ghost racer actors for %s"), *LevelName);
}
