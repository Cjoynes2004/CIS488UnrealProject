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

	MarbleRadius = 50.0f;
	// Dead zone and range are in Z-height units (cm). Tighter values make the ghost
	// respond more quickly as the player pulls ahead or falls behind.
	RubberBandDeadZone = 100.0f;
	RubberBandRange = 1000.0f;
	MinPlaybackSpeed = 0.5f;
	MaxPlaybackSpeed = 2.5f;

	AccumulatedRoll = FQuat::Identity;
	LastFrameLocation = FVector::ZeroVector;

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

	// Associate samples with moving platforms so the ghost follows them at runtime.
	SamplePlatforms.Init(nullptr, GhostRun.Samples.Num());
	SampleOffsets.Init(FVector::ZeroVector, GhostRun.Samples.Num());

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
	TArray<AActor*> MovingPlatformActors;
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetClass()->GetName().Contains(TEXT("MovingPlatform")))
		{
			MovingPlatformActors.Add(Actor);
		}
	}

	if (MovingPlatformActors.Num() > 0)
	{
		int32 AttachedCount = 0;
		for (int32 i = 0; i < GhostRun.Samples.Num(); i++)
		{
			const FVector SampleLoc = GhostRun.Samples[i].Location;
			AActor* BestPlatform = nullptr;
			float BestScore = FLT_MAX;
			for (AActor* Platform : MovingPlatformActors)
			{
				FVector PlatformLoc = Platform->GetActorLocation();
				FVector Origin, Extent;
				Platform->GetActorBounds(false, Origin, Extent);
				// Sample must be within the platform's horizontal footprint (+ some slack),
				// and just above the platform top.
				const float HorizSlack = 50.0f;
				if (FMath::Abs(SampleLoc.X - Origin.X) > Extent.X + HorizSlack) continue;
				if (FMath::Abs(SampleLoc.Y - Origin.Y) > Extent.Y + HorizSlack) continue;
				float PlatformTop = Origin.Z + Extent.Z;
				float VertDist = SampleLoc.Z - PlatformTop;
				if (VertDist < -50.0f || VertDist > 200.0f) continue;
				float Score = FVector::DistSquared(SampleLoc, PlatformLoc);
				if (Score < BestScore)
				{
					BestScore = Score;
					BestPlatform = Platform;
				}
			}
			if (BestPlatform)
			{
				SamplePlatforms[i] = BestPlatform;
				SampleOffsets[i] = SampleLoc - BestPlatform->GetActorLocation();
				AttachedCount++;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Ghost: Attached %d of %d samples to moving platforms"),
			AttachedCount, GhostRun.Samples.Num());
	}


	// Start hidden — the trigger button will call LoadAndPlay()
	SetActorHiddenInGame(true);
}

FVector AGhostRacerMarble::GetAdjustedSampleLocation(int32 SampleIdx) const
{
	if (SampleIdx >= 0 && SampleIdx < SamplePlatforms.Num() && SamplePlatforms[SampleIdx])
	{
		return SamplePlatforms[SampleIdx]->GetActorLocation() + SampleOffsets[SampleIdx];
	}
	if (SampleIdx >= 0 && SampleIdx < GhostRun.Samples.Num())
	{
		return GhostRun.Samples[SampleIdx].Location;
	}
	return FVector::ZeroVector;
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
		AccumulatedRoll = FQuat::Identity;
		LastFrameLocation = GetActorLocation();
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
	// Rubber-banding: use height difference as progress signal.
	// Level climbs upward, so higher Z means further along the race.
	// Player higher than ghost → ghost is behind in the climb → speed up to catch up.
	float SpeedScale = 1.0f;
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		FVector PlayerLoc = PlayerPawn->GetActorLocation();
		FVector GhostLoc = GetActorLocation();
		float HeightDiff = PlayerLoc.Z - GhostLoc.Z;
		float TotalDist = FVector::Dist(PlayerLoc, GhostLoc);

		// Don't rubber-band when ghost is >5000 units from player. At that range
		// they're on completely different sections; the Z-diff signal becomes noise.
		if (TotalDist < 5000.0f)
		{
			if (HeightDiff > RubberBandDeadZone)
			{
				float t = FMath::Clamp((HeightDiff - RubberBandDeadZone) / RubberBandRange, 0.0f, 1.0f);
				SpeedScale = FMath::Lerp(1.0f, MaxPlaybackSpeed, t);
			}
			else if (HeightDiff < -RubberBandDeadZone)
			{
				float t = FMath::Clamp((-HeightDiff - RubberBandDeadZone) / RubberBandRange, 0.0f, 1.0f);
				SpeedScale = FMath::Lerp(1.0f, MinPlaybackSpeed, t);
			}
		}

	}

	PlaybackTime += DeltaTime * SpeedScale;

	if (PlaybackTime >= GhostRun.FinalTime)
	{
		StopPlayback();
		return;
	}

	CurrentSampleIndex = FindSampleIndex(PlaybackTime);

	FVector NewLocation;
	if (CurrentSampleIndex >= GhostRun.Samples.Num() - 1)
	{
		NewLocation = GetAdjustedSampleLocation(GhostRun.Samples.Num() - 1);
		SetActorLocation(NewLocation);
	}
	else
	{
		const FGhostSample& SampleA = GhostRun.Samples[CurrentSampleIndex];
		const FGhostSample& SampleB = GhostRun.Samples[CurrentSampleIndex + 1];

		float TimeBetween = SampleB.Time - SampleA.Time;
		float Alpha = (TimeBetween > KINDA_SMALL_NUMBER)
			? FMath::Clamp((PlaybackTime - SampleA.Time) / TimeBetween, 0.0f, 1.0f)
			: 0.0f;

		// Use adjusted (moving-platform-tracked) locations as the interpolation endpoints,
		// then hermite-interpolate using the recorded velocities as tangents.
		FVector AdjA = GetAdjustedSampleLocation(CurrentSampleIndex);
		FVector AdjB = GetAdjustedSampleLocation(CurrentSampleIndex + 1);
		FVector TangentA = SampleA.Velocity * TimeBetween;
		FVector TangentB = SampleB.Velocity * TimeBetween;
		NewLocation = FMath::CubicInterp(AdjA, TangentA, AdjB, TangentB, Alpha);
		SetActorLocation(NewLocation);
	}

	// Rolling animation: spin the mesh so speed looks like rolling.
	// Axis = up × horizontal velocity, magnitude = |v| / radius.
	FVector FrameDelta = NewLocation - LastFrameLocation;
	FVector HorizDelta(FrameDelta.X, FrameDelta.Y, 0.0f);
	float HorizDist = HorizDelta.Size();
	if (HorizDist > KINDA_SMALL_NUMBER && DeltaTime > KINDA_SMALL_NUMBER && MarbleRadius > KINDA_SMALL_NUMBER)
	{
		FVector RollAxis = FVector::CrossProduct(FVector::UpVector, HorizDelta.GetSafeNormal());
		if (!RollAxis.IsNearlyZero())
		{
			RollAxis.Normalize();
			float AngularDelta = HorizDist / MarbleRadius;  // radians
			FQuat DeltaRot(RollAxis, AngularDelta);
			AccumulatedRoll = DeltaRot * AccumulatedRoll;
			AccumulatedRoll.Normalize();
			if (MarbleMesh)
			{
				MarbleMesh->SetRelativeRotation(AccumulatedRoll.Rotator());
			}
		}
	}
	LastFrameLocation = NewLocation;
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
			FVector(-1010, 60, 372),  // Cube
			FVector(-1730, 60, 532),  // Cube2
			FVector(-2510, 60, 632),  // Cube3
			FVector(-3400, 60, 695),  // Cube4
			FVector(-4500, -290, 695),  // Cube5
			FVector(-5820, -60, 632),  // Cube6
			FVector(-6210, 700, 712),  // Cube8
			FVector(-6210, 1810, 812),  // Cube9
			FVector(-5960, 3040, 962),  // Cube10
			FVector(-6420, 4180, 1042),  // Cube11
			FVector(-5980, 5320, 1092),  // Cube12
			FVector(-6430, 6570, 1202),  // Cube13
			FVector(-6040, 7880, 1295),  // Cube14
			FVector(-4450, 7910, 1392),  // Cube15
			FVector(-2420, 7880, 1445),  // Cube16
			FVector(-2420, 6060, 1397),  // Cube18
			FVector(-2040, 4240, 1557),  // Cube19
			FVector(-2220, 2440, 1763),  // Cube20
			FVector(-2280, 1660, 1933),  // Cube21
			FVector(-2220, 880, 2103),  // Cube22
			FVector(-2030, -790, 2132),  // Cube17
			FVector(-3380, -710, 2232),  // Cube23
			FVector(-4229, 251, 2371),  // Cube24
			FVector(-4579, 1401, 2441),  // Cube25
			FVector(-4449, 2731, 2501),  // Cube26
			FVector(-4079, 3931, 2571),  // Cube27
			FVector(-4269, 5091, 2661),  // Cube28
			FVector(-4939, 6081, 2824),  // Cube29
			FVector(-6002, 6340, 2881),  // CrumblingPlatform
			FVector(-7229, 5861, 2960),  // Cube31
			FVector(-7772, 4910, 2991),  // CrumblingPlatform2
			FVector(-7739, 3671, 3124),  // Cube33
			FVector(-6802, 2890, 3111),  // CrumblingPlatform3
			FVector(-5759, 2871, 3211),  // Cube61
			FVector(-4889, 2561, 3401),  // Cube62
			FVector(-3959, 2861, 3551),  // Cube41
			FVector(-3049, 2621, 3741),  // Cube42
			FVector(-2319, 3001, 3941),  // Cube43
			FVector(-1449, 2551, 4131),  // Cube44
			FVector(-769, 2001, 4241),  // Cube45
			FVector(-449, 1151, 4321),  // Cube46
			FVector(-930, 50, 4412),  // Cube47
			FVector(-2539, 171, 4522),  // Cube48
			FVector(-4230, -200, 4596),  // BouncePad
			FVector(-4230, -200, 5212),  // BouncePad apex (+500)
			FVector(-5789, -99, 4712),  // Cube50
			FVector(-7090, 770, 4596),  // BouncePad4
			FVector(-7090, 770, 5345),  // BouncePad4 apex (+500)
			FVector(-7359, 2281, 4845),  // Cube51
			FVector(-7760, 5920, 4772),  // Cube53
			FVector(-7040, 8750, 4772),  // Cube54
			FVector(-6450, 8750, 3736),  // Cube55
			FVector(-6130, 8750, 3548),  // Cube56
			FVector(-5770, 8750, 3432),  // Cube57
			FVector(-5410, 8750, 3509),  // Cube58
			FVector(-4940, 8750, 3870),  // Cube59
			FVector(-1030, 8780, 3765),  // Cube60
			FVector(-1040, 8220, 3776),  // MovingPlatform7
			FVector(298, 8980, 4341),  // CrumblingPlatform14
			FVector(-1040, 8220, 3776),  // MovingPlatform7
			FVector(1278, 7440, 4351),  // CrumblingPlatform15
			FVector(-1040, 8220, 3776),  // MovingPlatform7
			FVector(1940, 5810, 4086),  // BouncePad7
			FVector(1940, 5810, 5601),  // BouncePad7 apex (+500)
			FVector(1971, 5481, 5101),  // Cube63
			FVector(1370, 5670, 5150),  // Cylinder2
			FVector(1030, 5420, 5230),  // Cylinder3
			FVector(760, 5670, 5380),  // Cylinder4
			FVector(460, 5410, 5470),  // Cylinder5
			FVector(-990, 5020, 5385),  // Cylinder
			FVector(-1746, 6525, 6982),  // Cube64
			FVector(-2230, 7350, 7080),  // Cylinder6
			FVector(-2740, 7350, 7210),  // Cylinder7
			FVector(-3250, 7350, 7380),  // Cylinder8
			FVector(-3820, 7350, 7510),  // Cylinder9
			FVector(-4820, 7330, 7521),  // Cube30
			FVector(-5310, 7330, 7521),  // Cube32
			FVector(-5920, 6020, 7546),  // TiltingPlatform2
			FVector(-6450, 5480, 7696),  // TiltingPlatform
			FVector(-7060, 4910, 7846),  // TiltingPlatform3
			FVector(-7700, 4310, 7986),  // TiltingPlatform4
			FVector(-6740, 3320, 8041),  // Cube35
			FVector(-5332, 3280, 8139),  // Cube36 (loop inside)
			FVector(-5016, 3180, 8297),  // Cube37 (loop inside)
			FVector(-4734, 3090, 8490),  // Cube38 (loop inside)
			FVector(-4475, 3010, 8722),  // Cube39 (loop inside)
			FVector(-4251, 2940, 8973),  // Cube40 (loop inside)
			FVector(-4062, 2880, 9233),  // Cube65 (loop inside)
			FVector(-3893, 2840, 9522),  // Cube66 (loop inside)
			FVector(-3753, 2800, 9844),  // Cube67 (loop inside)
			FVector(-3651, 2790, 10194),  // Cube68 (loop inside)
			FVector(-3708, 2790, 10553),  // Cube69 (loop inside)
			FVector(-3821, 2780, 10887),  // Cube70 (loop inside)
			FVector(-3980, 2780, 11196),  // Cube71 (loop inside)
			FVector(-4202, 2780, 11471),  // Cube72 (loop inside)
			FVector(-4420, 2780, 11689),  // Cube73 (loop inside)
			FVector(-4662, 2780, 11882),  // Cube74 (loop inside)
			FVector(-4929, 2780, 12052),  // Cube75 (loop inside)
			FVector(-5241, 2780, 12199),  // Cube76 (loop inside)
			FVector(-5602, 2780, 12295),  // Cube77 (loop inside)
			FVector(-5966, 2780, 12205),  // Cube78 (loop inside)
			FVector(-6289, 2780, 12054),  // Cube79 (loop inside)
			FVector(-6582, 2780, 11871),  // Cube80 (loop inside)
			FVector(-6840, 2780, 11659),  // Cube81 (loop inside)
			FVector(-7067, 2780, 11434),  // Cube82 (loop inside)
			FVector(-7280, 2780, 11177),  // Cube83 (loop inside)
			FVector(-7465, 2780, 10883),  // Cube84 (loop inside)
			FVector(-7617, 2780, 10558),  // Cube85 (loop inside)
			FVector(-7703, 2780, 10202),  // Cube86 (loop inside)
			FVector(-7608, 2780, 9842),  // Cube87 (loop inside)
			FVector(-7461, 2780, 9520),  // Cube88 (loop inside)
			FVector(-7283, 2780, 9227),  // Cube89 (loop inside)
			FVector(-7091, 2780, 8983),  // Cube90 (loop inside)
			FVector(-6857, 2780, 8745),  // Cube91 (loop inside)
			FVector(-6611, 2770, 8557),  // Cube92 (loop inside)
			FVector(-6307, 2720, 8376),  // Cube93 (loop inside)
			FVector(-5994, 2650, 8222),  // Cube94 (loop inside)
			FVector(-4910, 2450, 8121),  // Cube95
			FVector(-3760, 2240, 8282),  // Cube98
			FVector(1630, 1000, 8262),  // Cube101
			FVector(1630, 2070, 8260),  // Cube102
			FVector(1630, 4210, 8260),  // Cube104
			FVector(1570, 5310, 8286),  // BouncePad12
			FVector(1570, 5310, 9980),  // BouncePad12 apex (+500)
			FVector(1570, 5310, 9480),  // Torus
			FVector(204, 4196, 9401),  // Cube96
			FVector(-970, 3218, 9520),  // Cube106
			FVector(-1318, 2924, 9757),  // Cube109
			FVector(-1571, 2712, 10063),  // Cube112
			FVector(-4900, -20, 11016),  // TiltingPlatform5
			FVector(-5561, 138, 11196),  // Cylinder16
			FVector(-6242, 190, 11371),  // CrumblingPlatform19
			FVector(-6341, 938, 11616),  // Cylinder17
			FVector(-6360, 1580, 11916),  // TiltingPlatform6
			FVector(-8032, 2770, 11475),  // BouncePad16
			FVector(-8032, 2770, 12961),  // BouncePad16 apex (+500)
			FVector(-5600, 2780, 12461),  // Cube77
			FVector(-5610, 3320, 12536),  // BouncePad15
			FVector(-5610, 3320, 13972),  // BouncePad15 apex (+500)
			FVector(-5620, 4410, 13472),  // Cube100
			FVector(-4270, 4400, 13616),  // ShrinkingPlatform
			FVector(-3180, 4410, 13682),  // Cube114
			FVector(-2050, 4390, 13876),  // ShrinkingPlatform2
			FVector(-934, 4348, 13886),  // MovingPlatform9
			FVector(160, 4410, 14002),  // Cube115
			FVector(50, 5880, 14096),  // ShrinkingPlatform3
			FVector(-410, 7130, 14158),  // Cube116
			FVector(-1070, 8110, 14288),  // Cube117
			FVector(-2200, 8760, 14488),  // Cube118
			FVector(-3550, 9160, 14666),  // ShrinkingPlatform4
			FVector(-4830, 8670, 14932),  // Cube119
			FVector(-5688, 7330, 15165),  // BouncePad17
			FVector(-5688, 7330, 15685),  // BouncePad17 apex (+500)
			FVector(-4402, 6280, 15185),  // BouncePad18
			FVector(-4402, 6280, 16006),  // BouncePad18 apex (+500)
			FVector(-5330, 5390, 15506),  // ShrinkingPlatform5
			FVector(-5620, 3980, 15642),  // Cube120
			FVector(-5590, 2570, 15816),  // BouncePad19
			FVector(-5590, 2570, 17016),  // BouncePad19 apex (+500)
			FVector(-5860, 1700, 16516),  // BouncePad20
			FVector(-5860, 1700, 17636),  // BouncePad20 apex (+500)
			FVector(-5080, 810, 17136),  // BouncePad21
			FVector(-5080, 810, 18116),  // BouncePad21 apex (+500)
			FVector(-3770, 1950, 17616),  // ShrinkingPlatform6
			FVector(-4600, 3700, 17622),  // Cube121
			FVector(-4650, 5230, 17746),  // ShrinkingPlatform7
			FVector(-4640, 6590, 17816),  // ShrinkingPlatform8
			FVector(-4640, 7840, 17906),  // ShrinkingPlatform9
			FVector(-4035, 9023, 18072),  // BouncePad22
			FVector(-4035, 9023, 18962),  // BouncePad22 apex (+500)
			FVector(-1960, 7810, 18462),  // Cube125
			FVector(-1980, 7020, 18462),  // Cube126
			FVector(-1980, 6430, 17426),  // Cube127
			FVector(-1980, 6110, 17238),  // Cube128
			FVector(-1980, 5750, 17122),  // Cube129
			FVector(-1980, 5390, 17199),  // Cube130
			FVector(-1980, 4920, 17560),  // Cube131
			FVector(-2208, -720, 17305),  // BouncePad23
			FVector(-2208, -720, 18415),  // BouncePad23 apex (+500)
			FVector(-820, -388, 17915),  // BouncePad24
			FVector(-820, -388, 18941),  // BouncePad24 apex (+500)
			FVector(-932, 1040, 18441),  // CrumblingPlatform21
			FVector(-552, 1770, 18551),  // CrumblingPlatform20
			FVector(35, 3480, 18549),  // Cube132
			FVector(-540, 3460, 18542),  // Cube133
			FVector(-900, 3460, 18619),  // Cube134
			FVector(-1357, 3460, 19073),  // Cube135
			FVector(-3458, 3470, 19375),  // BouncePad25
			FVector(-3458, 3470, 20985),  // BouncePad25 apex (+500)
			FVector(-1050, 3462, 20485),  // BouncePad26
			FVector(-1050, 3462, 21259),  // BouncePad26 apex (+500)
			FVector(-1045, 5780, 20759),  // Cube136
		};
	}

	if (PathPoints.Num() < 2)
	{
		return Run;
	}

	// Bounce pad locations (origin Z). Mesh top is ~86 above origin; ball center on top is +136.
	static const TArray<FVector> BouncePads = {
		FVector(-4230, -200, 4460), FVector(-1100, -850, 2010), FVector(1010, -850, 2010),
		FVector(-7090, 770, 4460), FVector(1010, 1050, 2010), FVector(-3120, -2540, 5320),
		FVector(1940, 5810, 3950), FVector(-4700, 8090, 7470), FVector(-3100, 8090, 7850),
		FVector(-4700, 8090, 8500), FVector(-3100, 8090, 9100), FVector(1570, 5310, 8150),
		FVector(-117, 7077, 9399), FVector(-2280, 8950, 9560), FVector(-5610, 3320, 12400),
		FVector(-8060, 2770, 11230), FVector(-5710, 7330, 14940), FVector(-4380, 6280, 14960),
		FVector(-5590, 2570, 15680), FVector(-5860, 1700, 16380), FVector(-5080, 810, 17000),
		FVector(-4050, 9030, 17840), FVector(-2230, -720, 17080), FVector(-820, -410, 17690),
		FVector(-3480, 3470, 19150), FVector(-1050, 3440, 20260)
	};
	static const float BouncePadBallZOffset = 136.0f;

	// Launch pad locations. Mesh top is ~26 above origin; ball center on top is +76.
	static const TArray<FVector> LaunchPads = {
		FVector(-4850, 8750, 3760), FVector(-3830, 2890, 9100), FVector(-3780, 2770, 11290),
		FVector(-4510, 2790, 12050), FVector(-5640, 2790, 12350), FVector(-5300, 3280, 8030),
		FVector(-3750, 2240, 8160), FVector(-6730, 2790, 12040), FVector(-7630, 2760, 10930),
		FVector(-7760, 2770, 10150), FVector(-1610, 2660, 9920), FVector(-1980, 4830, 17450),
		FVector(-1400, 3460, 18950), FVector(-450, 3460, 18500), FVector(-970, 3460, 18560)
	};
	static const float LaunchPadBallZOffset = 76.0f;

	// Before big Z jumps, insert a nearby pad (bounce or launch) so the ghost visibly touches it.
	auto FindClosestPad = [](const FVector& FromLoc, const TArray<FVector>& PadList, float ZOffset, FVector& OutBallPos) -> bool
	{
		// Widened from 1500 to 3000 so pads in adjacent sections can anchor long segments.
		// Minimum 200 units so we don't "insert" a pad that IS the current waypoint (duplicate).
		float BestHorizSqr = 3000.0f * 3000.0f;
		const float MinHorizSqr = 200.0f * 200.0f;
		int32 BestIdx = INDEX_NONE;
		for (int32 p = 0; p < PadList.Num(); p++)
		{
			const FVector& Pad = PadList[p];
			if (FMath::Abs(Pad.Z - FromLoc.Z) > 500.0f) continue;
			float HorizSqr = FVector(Pad.X - FromLoc.X, Pad.Y - FromLoc.Y, 0.0f).SizeSquared();
			if (HorizSqr < MinHorizSqr) continue;  // skip pads essentially AT the source waypoint
			if (HorizSqr < BestHorizSqr)
			{
				BestHorizSqr = HorizSqr;
				BestIdx = p;
			}
		}
		if (BestIdx != INDEX_NONE)
		{
			OutBallPos = PadList[BestIdx] + FVector(0.0f, 0.0f, ZOffset);
			return true;
		}
		return false;
	};

	// Auto pad-insertion disabled: the waypoint list is user-curated and already
	// includes the pads that should be visited in the correct order.

	// Subdivide long segments so the ghost doesn't make single arcs that span the map.
	// Each sub-segment gets its own arc/roll. 1500/800 gives arcs with apex ~300-500
	// for typical segments, which feels bouncy but not cinematic.
	const float MaxSegHoriz = 1500.0f;
	const float MaxSegVert = 800.0f;
	TArray<FVector> Subdivided;
	Subdivided.Reserve(PathPoints.Num() * 3);
	Subdivided.Add(PathPoints[0]);
	for (int32 i = 1; i < PathPoints.Num(); i++)
	{
		FVector A = PathPoints[i - 1];
		FVector B = PathPoints[i];
		FVector D = B - A;
		float Horiz = FVector(D.X, D.Y, 0.0f).Size();
		// Jump segments stay as a single clean projectile arc — subdividing them
		// creates stacked sub-arcs with discontinuous velocity at each boundary
		// (the "jagged" look). Only subdivide flat rolls, where sub-segments are
		// collinear and subdivision just adds sample points.
		bool bOrigJump = (D.Z > 30.0f) || (D.Z < -400.0f) || (Horiz > 2000.0f && D.Z < -100.0f);
		int32 Subs = bOrigJump ? 1 : FMath::Max(1, FMath::CeilToInt(Horiz / MaxSegHoriz));
		for (int32 s = 1; s <= Subs; s++)
		{
			float t = static_cast<float>(s) / Subs;
			Subdivided.Add(FMath::Lerp(A, B, t));
		}
	}
	PathPoints = Subdivided;

	float CurrentTime = 0.0f;
	// Match player's maxSpeed (1000) from Player_Marble.
	const float RollSpeed = 1000.0f;
	const float Gravity = 980.0f;
	const float SamplesPerSecond = 30.0f;

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

		// "Jump" uses projectile motion (with gravity). Ascents >50 and big drops >400 use it.
		// Gentle descents / flat moves use linear interp (rolling, ice sliding).
		// Big drops need projectile so the ball accelerates toward the ground instead of
		// sliding down at constant velocity (which looks floaty).
		bool bIsJump = (HeightDiff > 30.0f) || (HeightDiff < -400.0f) || (HorizDist > 2000.0f && HeightDiff < -100.0f);

		if (bIsJump)
		{
			float FlightTime = FMath::Clamp(HorizDist / RollSpeed, 0.3f, 2.0f);
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
