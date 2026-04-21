#include "GhostStartTrigger.h"
#include "GhostRacerMarble.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Sound/SoundWave.h"

AGhostStartTrigger::AGhostStartTrigger()
{
	PrimaryActorTick.bCanEverTick = true;

	RaceState = ERaceState::WaitingForTrigger;
	GhostMarble = nullptr;
	TriggerRadius = 200.0f;
	ButtonColor = FLinearColor(0.0f, 1.0f, 0.3f, 1.0f);
	CountdownDuration = 3.0f;
	CountdownTimer = 0.0f;
	LastDisplayedNumber = 0;
	CachedPlayerPawn = nullptr;
	CachedPlayerPhysicsComp = nullptr;
	CountdownTickSound = nullptr;
	CountdownGoSound = nullptr;
	RollingSound = nullptr;
	LandingSound = nullptr;
	CountdownAudioComp = nullptr;
	RollingAudioComp = nullptr;
	BGMAudioComp = nullptr;
	bPlayerWasInAir = false;

	// Side-by-side at first platform
	PlayerStartPosition = FVector(-780.0f, -40.0f, 400.0f);
	GhostStartPosition = FVector(-780.0f, 160.0f, 400.0f);

	// Visible button platform
	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ButtonMesh->SetWorldScale3D(FVector(1.5f, 1.5f, 0.1f));
	RootComponent = ButtonMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ButtonMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AGhostStartTrigger::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find the ghost marble
	if (!GhostMarble)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGhostRacerMarble::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			GhostMarble = Cast<AGhostRacerMarble>(Found[0]);
		}
	}

	// Load sounds
	CountdownTickSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Sounds/SFX/Countdown/countdown_04.countdown_04"));
	CountdownGoSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Sounds/SFX/Countdown/countdown_02.countdown_02"));
	RollingSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Sounds/SFX/Rolling/normal_sfx_rolling.normal_sfx_rolling"));
	if (RollingSound)
	{
		RollingSound->bLooping = true;
	}
	LandingSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Sounds/SFX/Landing/normal_sfx_landing.normal_sfx_landing"));

	// Start BGM
	USoundWave* BGMSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Sounds/Music/Tutorial/tutorial_bgm_01.tutorial_bgm_01"));
	if (BGMSound)
	{
		BGMAudioComp = UGameplayStatics::SpawnSound2D(GetWorld(), BGMSound, 0.15f, 1.0f, 0.0f);
	}

	// Apply button color with glow
	if (ButtonMesh)
	{
		UMaterialInterface* BaseMat = ButtonMesh->GetMaterial(0);
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), ButtonColor);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), ButtonColor * 2.0f);
			ButtonMesh->SetMaterial(0, DynMat);
		}
	}
}

void AGhostStartTrigger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (RaceState)
	{
	case ERaceState::WaitingForTrigger:
		TickWaitingForTrigger(DeltaTime);
		UpdatePlayerAudio();
		break;
	case ERaceState::Countdown:
		TickCountdown(DeltaTime);
		break;
	case ERaceState::Racing:
		UpdatePlayerAudio();
		break;
	case ERaceState::Finished:
		break;
	}
}

void AGhostStartTrigger::TickWaitingForTrigger(float DeltaTime)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	UPrimitiveComponent* PhysComp = FindPlayerPhysicsComp(PlayerPawn);
	FVector PlayerPos = PhysComp ? PhysComp->GetComponentLocation() : PlayerPawn->GetActorLocation();

	float Distance = FVector::Dist(GetActorLocation(), PlayerPos);

	if (Distance <= TriggerRadius)
	{
		// Cache player references
		CachedPlayerPawn = PlayerPawn;
		CachedPlayerPhysicsComp = PhysComp;

		// Stop rolling sound during countdown
		if (RollingAudioComp)
		{
			RollingAudioComp->Stop();
			RollingAudioComp = nullptr;
		}

		// Freeze player: stop physics and input
		if (CachedPlayerPhysicsComp)
		{
			CachedPlayerPhysicsComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
			CachedPlayerPhysicsComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			CachedPlayerPhysicsComp->SetSimulatePhysics(false);
		}

		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			PC->SetIgnoreMoveInput(true);
		}

		// Teleport player and ghost side-by-side
		PlayerPawn->SetActorLocation(PlayerStartPosition);
		if (CachedPlayerPhysicsComp)
		{
			CachedPlayerPhysicsComp->SetWorldLocation(PlayerStartPosition);
		}

		if (GhostMarble)
		{
			GhostMarble->SetActorLocation(GhostStartPosition);
			GhostMarble->SetActorHiddenInGame(false);
		}

		// Dim button and start countdown
		DimButton();
		CountdownTimer = CountdownDuration;
		LastDisplayedNumber = 0;
		RaceState = ERaceState::Countdown;
	}
}

void AGhostStartTrigger::TickCountdown(float DeltaTime)
{
	CountdownTimer -= DeltaTime;

	if (CountdownTimer > 0.0f)
	{
		int32 DisplayNumber = FMath::CeilToInt(CountdownTimer);
		FString CountdownText = FString::Printf(TEXT("%d"), DisplayNumber);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(42, 0.5f, FColor::Yellow,
				CountdownText, true, FVector2D(5.0f, 5.0f));
		}

		// Play tick sound on each number change — stop previous first
		if (DisplayNumber != LastDisplayedNumber)
		{
			LastDisplayedNumber = DisplayNumber;
			if (CountdownAudioComp)
			{
				CountdownAudioComp->Stop();
			}
			if (CountdownTickSound)
			{
				CountdownAudioComp = UGameplayStatics::SpawnSound2D(GetWorld(), CountdownTickSound, 0.5f);
			}
		}
	}
	else
	{
		// Stop any lingering countdown tick
		if (CountdownAudioComp)
		{
			CountdownAudioComp->Stop();
			CountdownAudioComp = nullptr;
		}

		// GO!
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(42, 1.5f, FColor::Green,
				TEXT("GO!"), true, FVector2D(5.0f, 5.0f));
		}

		if (CountdownGoSound)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), CountdownGoSound, 0.6f);
		}

		// Unfreeze player
		if (CachedPlayerPhysicsComp)
		{
			CachedPlayerPhysicsComp->SetSimulatePhysics(true);
		}

		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			PC->SetIgnoreMoveInput(false);
		}

		// Start ghost playback
		if (GhostMarble)
		{
			GhostMarble->LoadAndPlay();
		}

		RaceState = ERaceState::Racing;
	}
}

void AGhostStartTrigger::UpdatePlayerAudio()
{
	APawn* PlayerPawn = CachedPlayerPawn;
	if (!PlayerPawn)
	{
		PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	}
	if (!PlayerPawn) return;

	UPrimitiveComponent* PhysComp = CachedPlayerPhysicsComp;
	if (!PhysComp)
	{
		PhysComp = FindPlayerPhysicsComp(PlayerPawn);
	}
	if (!PhysComp) return;

	FVector Velocity = PhysComp->GetPhysicsLinearVelocity();
	float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
	float DT = GetWorld()->GetDeltaSeconds();

	// Use a line trace to detect actual ground contact
	FVector TraceStart = PhysComp->GetComponentLocation();
	FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 80.0f);
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedPlayerPawn ? CachedPlayerPawn : UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	bool bConfirmedOnGround = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
	bool bIsFalling = Velocity.Z < -150.0f;

	// Rolling sound
	if (HorizontalSpeed > 50.0f && bConfirmedOnGround)
	{
		if (!RollingAudioComp && RollingSound)
		{
			float Vol = FMath::Clamp(HorizontalSpeed / 800.0f, 0.1f, 0.5f);
			RollingAudioComp = UGameplayStatics::SpawnSound2D(GetWorld(), RollingSound, Vol);
		}
		else if (RollingAudioComp)
		{
			float Vol = FMath::Clamp(HorizontalSpeed / 800.0f, 0.1f, 0.5f);
			RollingAudioComp->SetVolumeMultiplier(Vol);
		}
	}
	else if (RollingAudioComp)
	{
		RollingAudioComp->FadeOut(0.15f, 0.0f);
		RollingAudioComp = nullptr;
	}

	// Landing sound: only after sustained falling, then hitting ground
	static float FallingTime = 0.0f;
	static float LandingCooldown = 0.0f;

	if (LandingCooldown > 0.0f)
	{
		LandingCooldown -= DT;
	}

	if (bIsFalling)
	{
		FallingTime += DT;
	}
	else if (bConfirmedOnGround)
	{
		// Only trigger landing if we were falling for at least 0.15 seconds
		if (FallingTime > 0.15f && LandingCooldown <= 0.0f)
		{
			if (LandingSound)
			{
				UGameplayStatics::PlaySoundAtLocation(
					GetWorld(), LandingSound, PhysComp->GetComponentLocation(), 0.25f);
				LandingCooldown = 0.5f;
			}
		}
		FallingTime = 0.0f;
	}
}

void AGhostStartTrigger::DimButton()
{
	if (ButtonMesh)
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(ButtonMesh->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
		}
	}
}

UPrimitiveComponent* AGhostStartTrigger::FindPlayerPhysicsComp(APawn* Pawn) const
{
	if (!Pawn) return nullptr;

	TArray<UPrimitiveComponent*> PrimComps;
	Pawn->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Comp : PrimComps)
	{
		if (Comp->IsSimulatingPhysics())
		{
			return Comp;
		}
	}
	return nullptr;
}
