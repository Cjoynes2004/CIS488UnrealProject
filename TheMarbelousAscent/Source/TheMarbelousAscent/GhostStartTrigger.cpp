#include "GhostStartTrigger.h"
#include "GhostRacerMarble.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"

AGhostStartTrigger::AGhostStartTrigger()
{
	PrimaryActorTick.bCanEverTick = true;

	RaceState = ERaceState::WaitingForTrigger;
	GhostMarble = nullptr;
	TriggerRadius = 200.0f;
	ButtonColor = FLinearColor(0.0f, 1.0f, 0.3f, 1.0f);
	CountdownDuration = 3.0f;
	CountdownTimer = 0.0f;
	CachedPlayerPawn = nullptr;
	CachedPlayerPhysicsComp = nullptr;

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
		break;
	case ERaceState::Countdown:
		TickCountdown(DeltaTime);
		break;
	case ERaceState::Racing:
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
	}
	else
	{
		// GO!
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(42, 1.5f, FColor::Green,
				TEXT("GO!"), true, FVector2D(5.0f, 5.0f));
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
