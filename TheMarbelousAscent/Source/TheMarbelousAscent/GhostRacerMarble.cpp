#include "GhostRacerMarble.h"
#include "GhostSaveGame.h"
#include "RaceWaypoint.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

AGhostRacerMarble::AGhostRacerMarble()
{
	PrimaryActorTick.bCanEverTick = true;

	GhostOpacity = 0.5f;
	GhostColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);  // Light blue
	GlowIntensity = 1.5f;
	bIsPlaying = false;
	bGhostDataReady = false;
	PlaybackTime = 0.0f;
	CurrentSampleIndex = 0;

	// Non-physics sphere — no collision at all so it never interacts with the player
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetSphereRadius(50.0f);
	SphereCollision->SetSimulatePhysics(false);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereCollision->SetGenerateOverlapEvents(false);
	RootComponent = SphereCollision;

	// Visual mesh — also no collision
	MarbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarbleMesh"));
	MarbleMesh->SetupAttachment(SphereCollision);
	MarbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarbleMesh->SetGenerateOverlapEvents(false);
	MarbleMesh->SetForceDisableNanite(true);  // Nanite doesn't support translucent/additive materials

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MarbleMesh->SetStaticMesh(SphereMesh.Object);
		MarbleMesh->SetWorldScale3D(FVector(1.0f));
	}
}

void AGhostRacerMarble::BeginPlay()
{
	Super::BeginPlay();

	ApplyGhostAppearance();

	// Preload ghost data but do NOT start playing — wait for trigger
	FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(GetWorld());

	// Try saved player ghost first
	if (UGameplayStatics::DoesSaveGameExist(UGhostSaveGame::GetSlotName(), 0))
	{
		UGhostSaveGame* SaveGame = Cast<UGhostSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UGhostSaveGame::GetSlotName(), 0));
		if (SaveGame)
		{
			FGhostRun* SavedRun = SaveGame->BestRuns.Find(CurrentLevel);
			if (SavedRun && SavedRun->IsValid())
			{
				GhostRun = *SavedRun;
				bGhostDataReady = true;
				UE_LOG(LogTemp, Log, TEXT("Ghost: Loaded saved player ghost for %s (%.2fs)"),
					*CurrentLevel, GhostRun.FinalTime);
			}
		}
	}

	// Fall back to staff ghost from waypoints
	if (!bGhostDataReady)
	{
		GhostRun = GenerateStaffGhost();
		bGhostDataReady = GhostRun.IsValid();
		if (bGhostDataReady)
		{
			UE_LOG(LogTemp, Log, TEXT("Ghost: Generated staff ghost for %s (%.2fs, %d samples)"),
				*CurrentLevel, GhostRun.FinalTime, GhostRun.Samples.Num());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Ghost: No ghost data available for %s"), *CurrentLevel);
		}
	}

	// Start hidden — the trigger button will call LoadAndPlay()
	SetActorHiddenInGame(true);
}

void AGhostRacerMarble::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPlaying)
	{
		UpdatePlayback(DeltaTime);
	}
}

void AGhostRacerMarble::StartPlayback(const FGhostRun& Run)
{
	if (!Run.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ghost: Cannot play invalid run"));
		return;
	}

	GhostRun = Run;
	bGhostDataReady = true;
	bIsPlaying = true;
	PlaybackTime = 0.0f;
	CurrentSampleIndex = 0;

	SetActorLocation(GhostRun.Samples[0].Location);
	SetActorRotation(GhostRun.Samples[0].Rotation);
	SetActorHiddenInGame(false);

	UE_LOG(LogTemp, Log, TEXT("Ghost: Started playback (%d samples, %.2fs)"),
		GhostRun.Samples.Num(), GhostRun.FinalTime);
}

void AGhostRacerMarble::StopPlayback()
{
	bIsPlaying = false;
	SetActorHiddenInGame(true);
	UE_LOG(LogTemp, Log, TEXT("Ghost: Stopped playback"));
}

void AGhostRacerMarble::LoadAndPlay()
{
	if (bGhostDataReady && GhostRun.IsValid())
	{
		// Insert current position as the first sample so playback starts from
		// where the trigger placed us, not from the hardcoded path origin
		FGhostSample StartSample;
		StartSample.Time = 0.0f;
		StartSample.Location = GetActorLocation();
		StartSample.Rotation = GetActorRotation();
		StartSample.Velocity = FVector::ZeroVector;

		// Shift all existing sample times forward by a small transition period
		const float TransitionTime = 0.5f;
		for (FGhostSample& S : GhostRun.Samples)
		{
			S.Time += TransitionTime;
		}
		GhostRun.Samples.Insert(StartSample, 0);
		GhostRun.FinalTime += TransitionTime;

		bIsPlaying = true;
		PlaybackTime = 0.0f;
		CurrentSampleIndex = 0;
		SetActorHiddenInGame(false);

		UE_LOG(LogTemp, Log, TEXT("Ghost: Started playback (%d samples, %.2fs)"),
			GhostRun.Samples.Num(), GhostRun.FinalTime);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Ghost: No ghost data ready to play"));
	}
}

float AGhostRacerMarble::GetProgress() const
{
	if (!bIsPlaying || GhostRun.FinalTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(PlaybackTime / GhostRun.FinalTime, 0.0f, 1.0f);
}

void AGhostRacerMarble::UpdatePlayback(float DeltaTime)
{
	PlaybackTime += DeltaTime;

	if (PlaybackTime >= GhostRun.FinalTime)
	{
		StopPlayback();
		return;
	}

	CurrentSampleIndex = FindSampleIndex(PlaybackTime);

	if (CurrentSampleIndex >= GhostRun.Samples.Num() - 1)
	{
		const FGhostSample& Last = GhostRun.Samples.Last();
		SetActorLocation(Last.Location);
		SetActorRotation(Last.Rotation);
		return;
	}

	const FGhostSample& SampleA = GhostRun.Samples[CurrentSampleIndex];
	const FGhostSample& SampleB = GhostRun.Samples[CurrentSampleIndex + 1];

	float TimeBetween = SampleB.Time - SampleA.Time;
	float Alpha = (TimeBetween > KINDA_SMALL_NUMBER)
		? FMath::Clamp((PlaybackTime - SampleA.Time) / TimeBetween, 0.0f, 1.0f)
		: 0.0f;

	FVector InterpolatedLocation = HermiteInterpolateLocation(SampleA, SampleB, Alpha);
	FRotator InterpolatedRotation = FMath::Lerp(SampleA.Rotation, SampleB.Rotation, Alpha);

	SetActorLocation(InterpolatedLocation);
	SetActorRotation(InterpolatedRotation);
}

void AGhostRacerMarble::ApplyGhostAppearance()
{
	if (!MarbleMesh)
	{
		return;
	}

	// Use Blue material for now — translucent materials don't render correctly in this project
	UMaterialInterface* GhostMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Blue.Blue"));
	if (GhostMat)
	{
		MarbleMesh->SetMaterial(0, GhostMat);
	}
	MarbleMesh->SetCastShadow(false);
}

int32 AGhostRacerMarble::FindSampleIndex(float Time) const
{
	int32 Low = 0;
	int32 High = GhostRun.Samples.Num() - 1;

	while (Low < High - 1)
	{
		int32 Mid = (Low + High) / 2;
		if (GhostRun.Samples[Mid].Time <= Time)
		{
			Low = Mid;
		}
		else
		{
			High = Mid;
		}
	}

	return Low;
}

FVector AGhostRacerMarble::HermiteInterpolateLocation(const FGhostSample& A, const FGhostSample& B, float Alpha) const
{
	float TimeBetween = B.Time - A.Time;

	FVector TangentA = A.Velocity * TimeBetween;
	FVector TangentB = B.Velocity * TimeBetween;

	return FMath::CubicInterp(A.Location, TangentA, B.Location, TangentB, Alpha);
}

FGhostRun AGhostRacerMarble::GenerateStaffGhost() const
{
	FGhostRun Run;
	Run.LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld());

	// First try RaceWaypoint actors in the level
	TArray<FVector> PathPoints;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARaceWaypoint::StaticClass(), FoundActors);

	if (FoundActors.Num() >= 2)
	{
		TArray<ARaceWaypoint*> Waypoints;
		for (AActor* Actor : FoundActors)
		{
			ARaceWaypoint* WP = Cast<ARaceWaypoint>(Actor);
			if (WP)
			{
				Waypoints.Add(WP);
			}
		}
		Waypoints.Sort([](const ARaceWaypoint& A, const ARaceWaypoint& B)
		{
			return A.WaypointIndex < B.WaypointIndex;
		});
		for (ARaceWaypoint* WP : Waypoints)
		{
			PathPoints.Add(WP->GetActorLocation());
		}
	}
	else
	{
		// Hardcoded path for TutorialTest level — positions on top of each platform
		PathPoints = {
			FVector(-780, 60, 370),
			FVector(-1010, 60, 530),
			FVector(-1730, 60, 690),
			FVector(-2510, 60, 790),
			FVector(-3400, 60, 790),
			FVector(-4500, -290, 790),
			FVector(-5820, -60, 790),
			FVector(-6210, 700, 870),
			FVector(-6210, 1810, 970),
			FVector(-5960, 3040, 1120),
			FVector(-6420, 4180, 1042),
			FVector(-5980, 5320, 1092),
			FVector(-6430, 6570, 1202),
			FVector(-6040, 7880, 1295),
			FVector(-4450, 7910, 1392),
			FVector(-2420, 7880, 1445),
			FVector(-2420, 6060, 1430),
			FVector(-2040, 4240, 1590),
			FVector(-2220, 2440, 1700),
			FVector(-2280, 1660, 1870),
			FVector(-2220, 880, 2040),
			FVector(-2030, -790, 2132),
			FVector(-3380, -710, 2232),
			FVector(-4229, 251, 2371),
			FVector(-4579, 1401, 2441),
			FVector(-4449, 2731, 2501),
			FVector(-4079, 3931, 2571),
			FVector(-4269, 5091, 2661),
			FVector(-4939, 6081, 2751),
			FVector(-6039, 6431, 2841),
			FVector(-7229, 5861, 2921),
			FVector(-7809, 4901, 3011),
			FVector(-7739, 3671, 3051),
			FVector(-6829, 2881, 3101),
			FVector(-5759, 2871, 3211),
			FVector(-4889, 2561, 3401),
			FVector(-3999, 2861, 3551),
			FVector(-3129, 2551, 3741),
			FVector(-2319, 3001, 3941),
			FVector(-1449, 2551, 4131),
			FVector(-769, 2001, 4241),
			FVector(-449, 1151, 4321),
			FVector(-930, 50, 4412)
		};
	}

	if (PathPoints.Num() < 2)
	{
		return Run;
	}

	float CurrentTime = 0.0f;
	const float RollSpeed = 800.0f;
	const float Gravity = 980.0f;
	const float SamplesPerSecond = 20.0f;

	for (int32 i = 0; i < PathPoints.Num(); i++)
	{
		FVector WPLocation = PathPoints[i];

		if (i == 0)
		{
			FGhostSample Sample;
			Sample.Time = 0.0f;
			Sample.Location = WPLocation;
			Sample.Rotation = FRotator::ZeroRotator;
			Sample.Velocity = FVector::ZeroVector;
			Run.Samples.Add(Sample);
			continue;
		}

		FVector PrevLocation = PathPoints[i - 1];
		FVector Delta = WPLocation - PrevLocation;
		float HorizDist = FVector(Delta.X, Delta.Y, 0.0f).Size();
		float HeightDiff = Delta.Z;

		bool bIsJump = (HeightDiff > 30.0f) || (HorizDist > 600.0f);

		if (bIsJump)
		{
			float FlightTime = FMath::Clamp(HorizDist / RollSpeed, 0.6f, 2.5f);
			float Vx = HorizDist / FlightTime;
			float Vz = (HeightDiff + 0.5f * Gravity * FlightTime * FlightTime) / FlightTime;
			FVector HorizDir = FVector(Delta.X, Delta.Y, 0.0f).GetSafeNormal();

			int32 NumSamples = FMath::Max(static_cast<int32>(FlightTime * SamplesPerSecond), 4);
			float TimeStep = FlightTime / NumSamples;

			for (int32 s = 1; s <= NumSamples; s++)
			{
				float t = TimeStep * s;
				FGhostSample Sample;
				Sample.Time = CurrentTime + t;
				Sample.Location = PrevLocation + HorizDir * (Vx * t)
					+ FVector(0.0f, 0.0f, Vz * t - 0.5f * Gravity * t * t);
				Sample.Velocity = HorizDir * Vx + FVector(0.0f, 0.0f, Vz - Gravity * t);
				Sample.Rotation = Sample.Velocity.Rotation();
				Run.Samples.Add(Sample);
			}

			CurrentTime += FlightTime;
		}
		else
		{
			float RollTime = HorizDist / RollSpeed;
			RollTime = FMath::Max(RollTime, 0.3f);
			FVector RollDir = Delta.GetSafeNormal();
			FVector RollVel = RollDir * RollSpeed;

			int32 NumSamples = FMath::Max(static_cast<int32>(RollTime * SamplesPerSecond), 2);
			float TimeStep = RollTime / NumSamples;

			for (int32 s = 1; s <= NumSamples; s++)
			{
				float t = TimeStep * s;
				FGhostSample Sample;
				Sample.Time = CurrentTime + t;
				Sample.Location = FMath::Lerp(PrevLocation, WPLocation, static_cast<float>(s) / NumSamples);
				Sample.Velocity = RollVel;
				Sample.Rotation = RollDir.Rotation();
				Run.Samples.Add(Sample);
			}

			CurrentTime += RollTime;
		}
	}

	Run.FinalTime = CurrentTime;

	UE_LOG(LogTemp, Log, TEXT("Ghost: Generated staff ghost with %d samples over %.1fs from %d waypoints"),
		Run.Samples.Num(), Run.FinalTime, PathPoints.Num());

	return Run;
}
