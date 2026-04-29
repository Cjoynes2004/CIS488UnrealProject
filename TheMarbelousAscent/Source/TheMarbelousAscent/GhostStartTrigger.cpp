#include "GhostStartTrigger.h"
#include "GhostRacerMarble.h"
#include "RaceCountdownWidget.h"
#include "RaceResultWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Sound/SoundWave.h"
#include "Blueprint/UserWidget.h"

AGhostStartTrigger::AGhostStartTrigger()
{
	PrimaryActorTick.bCanEverTick = true;

	RaceState = ERaceState::WaitingForTrigger;
	GhostMarble = nullptr;
	TriggerRadius = 200.0f;
	ButtonColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);
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
	CountdownWidget = nullptr;
	ResultWidget = nullptr;
	bPlayerWasInAir = false;
	RaceStartGameTime = 0.0f;
	ButtonPulseTime = 0.0f;
	DebugScreenshotTimer = 0.0f;
	DebugScreenshotCount = 0;

	// Side-by-side at first platform
	PlayerStartPosition = FVector(-780.0f, -40.0f, 400.0f);
	GhostStartPosition = FVector(-780.0f, 160.0f, 400.0f);

	// Finish line: Cube136 sits at (-1045, 5780, ~20760).
	FinishLineLocation = FVector(-1045.0f, 5780.0f, 20760.0f);
	FinishLineRadius = 400.0f;

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

	// Point light supplies the visible "glow" since cylinder mesh materials
	// rarely expose an emissive param. Pulse intensity in UpdateButtonPulse.
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(ButtonMesh);
	GlowLight->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	GlowLight->SetAttenuationRadius(700.0f);
	GlowLight->SetSourceRadius(40.0f);
	GlowLight->SetCastShadows(false);
	GlowLight->SetIntensity(2500.0f);
	GlowLight->SetLightColor(ButtonColor);
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

	// Apply a project material that's actually red on the cylinder. The
	// "glow" is provided by the attached PointLight; the mesh just needs a
	// solid red surface that catches the light convincingly.
	if (ButtonMesh)
	{
		UMaterialInterface* BaseMat =
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Red.Red"));
		if (BaseMat)
		{
			ButtonMesh->SetMaterial(0, BaseMat);
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
		UpdateButtonPulse(DeltaTime);
		UpdatePlayerAudio();
		break;
	case ERaceState::Countdown:
		TickCountdown(DeltaTime);
		break;
	case ERaceState::Racing:
		TickRacing(DeltaTime);
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

		// Bring up the countdown HUD. Use the user-supplied widget class if
		// one was provided (lets designers override visuals), otherwise spawn
		// the built-in self-constructing widget.
		if (PC)
		{
			UClass* WidgetClass = CountdownWidgetClass ? CountdownWidgetClass.Get() : URaceCountdownWidget::StaticClass();
			CountdownWidget = CreateWidget<URaceCountdownWidget>(PC, WidgetClass);
			if (CountdownWidget)
			{
				CountdownWidget->AddToViewport(50);
			}
		}
	}
}

void AGhostStartTrigger::TickCountdown(float DeltaTime)
{
	CountdownTimer -= DeltaTime;

	if (CountdownTimer > 0.0f)
	{
		int32 DisplayNumber = FMath::CeilToInt(CountdownTimer);

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

			if (CountdownWidget)
			{
				CountdownWidget->ShowNumber(DisplayNumber);
			}
			else if (GEngine)
			{
				FString CountdownText = FString::Printf(TEXT("%d"), DisplayNumber);
				GEngine->AddOnScreenDebugMessage(42, 0.5f, FColor::Yellow,
					CountdownText, true, FVector2D(5.0f, 5.0f));
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
		if (CountdownWidget)
		{
			CountdownWidget->ShowGo();
			// Tear it down after a short grace period.
			FTimerHandle TimerHandle;
			TWeakObjectPtr<URaceCountdownWidget> WidgetPtr = CountdownWidget;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle,
				FTimerDelegate::CreateLambda([WidgetPtr]()
				{
					if (WidgetPtr.IsValid())
					{
						WidgetPtr->RemoveFromParent();
					}
				}),
				1.5f, false);
			CountdownWidget = nullptr;
		}
		else if (GEngine)
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

		RaceStartGameTime = GetWorld()->GetTimeSeconds();
		RaceState = ERaceState::Racing;
	}
}

void AGhostStartTrigger::TickRacing(float DeltaTime)
{
	APawn* PlayerPawn = CachedPlayerPawn ? CachedPlayerPawn : UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn)
	{
		return;
	}

	UPrimitiveComponent* PhysComp = CachedPlayerPhysicsComp;
	if (!PhysComp)
	{
		PhysComp = FindPlayerPhysicsComp(PlayerPawn);
	}
	const FVector PlayerPos = PhysComp ? PhysComp->GetComponentLocation() : PlayerPawn->GetActorLocation();

	if (FVector::Dist(PlayerPos, FinishLineLocation) <= FinishLineRadius)
	{
		// Player won iff the ghost is still mid-playback.
		const bool bPlayerWon = GhostMarble && GhostMarble->IsPlaying();
		FinishRace(bPlayerWon);
		return;
	}

	// If the ghost finishes its run before the player crosses the line, end
	// the race immediately as a loss so the player gets the result screen
	// without having to keep rolling to Cube136.
	if (GhostMarble && !GhostMarble->IsPlaying())
	{
		FinishRace(false);
	}
}

void AGhostStartTrigger::FinishRace(bool bPlayerWon)
{
	// Idempotent: once the race has been called, never re-call. Prevents a
	// "ghost wins" screen from appearing after the player already won.
	if (RaceState == ERaceState::Finished)
	{
		return;
	}

	const float PlayerSeconds = GetWorld()->GetTimeSeconds() - RaceStartGameTime;
	// "Ghost finished" means it ran the entire path before the player crossed
	// the goal. If it was still mid-playback, its elapsed time is meaningless
	// to the player and we hide it from the result widget.
	const bool bGhostFinished = GhostMarble && !GhostMarble->IsPlaying();
	const float GhostSeconds = GhostMarble ? GhostMarble->GetElapsedTime() : 0.0f;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		UClass* WidgetClass = ResultWidgetClass ? ResultWidgetClass.Get() : URaceResultWidget::StaticClass();
		ResultWidget = CreateWidget<URaceResultWidget>(PC, WidgetClass);
		if (ResultWidget)
		{
			ResultWidget->AddToViewport(60);
			ResultWidget->ShowResult(bPlayerWon, PlayerSeconds, GhostSeconds, bGhostFinished);
		}
	}

	if (GhostMarble && GhostMarble->IsPlaying())
	{
		GhostMarble->StopPlayback();
	}

	// Auto-dismiss the result widget after 20 seconds so it doesn't sit on
	// the screen forever.
	FTimerHandle DismissHandle;
	TWeakObjectPtr<URaceResultWidget> WidgetPtr = ResultWidget;
	GetWorld()->GetTimerManager().SetTimer(DismissHandle,
		FTimerDelegate::CreateLambda([WidgetPtr]()
		{
			if (WidgetPtr.IsValid())
			{
				WidgetPtr->RemoveFromParent();
			}
		}),
		20.0f, false);

	RaceState = ERaceState::Finished;
}

void AGhostStartTrigger::UpdateButtonPulse(float DeltaTime)
{
	if (!GlowLight)
	{
		return;
	}

	// Soft pulse on the point light only — no scale change, no color change.
	// Intensity sweeps between 1500 and 4000 lumens at 1.2 Hz, with the
	// light's color tracking ButtonColor so the glow is unmistakably red.
	ButtonPulseTime += DeltaTime;
	const float PulseHz = 1.2f;
	const float T = 0.5f * (1.0f + FMath::Sin(ButtonPulseTime * PulseHz * 2.0f * PI));
	const float Intensity = FMath::Lerp(1500.0f, 4000.0f, T);
	GlowLight->SetIntensity(Intensity);
	GlowLight->SetLightColor(ButtonColor);
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
	// Once the player has activated the trigger, kill the glow light and
	// swap the button mesh to a darker material so it visually reads as
	// "consumed" rather than "active".
	if (GlowLight)
	{
		GlowLight->SetIntensity(0.0f);
		GlowLight->SetVisibility(false);
	}
	if (ButtonMesh)
	{
		if (UMaterialInterface* DimMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Materials/Black.Black")))
		{
			ButtonMesh->SetMaterial(0, DimMat);
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
