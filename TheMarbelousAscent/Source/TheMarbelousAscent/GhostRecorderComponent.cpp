#include "GhostRecorderComponent.h"
#include "GhostSaveGame.h"
#include "Kismet/GameplayStatics.h"

UGhostRecorderComponent::UGhostRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	GroundSampleRate = 15.0f;  // 15 Hz on ground
	AirSampleRate = 30.0f;     // 30 Hz in air
	bIsRecording = false;
	RecordTimer = 0.0f;
	LastRecordedLocation = FVector::ZeroVector;
	LastRecordedRotation = FRotator::ZeroRotator;
}

void UGhostRecorderComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGhostRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRecording)
	{
		return;
	}

	RecordTimer += DeltaTime;

	float SampleInterval = GetCurrentSampleInterval();

	// Record at the target rate, or immediately if a forced sample is needed
	if (RecordTimer >= SampleInterval || ShouldForceSample())
	{
		RecordSample();
		RecordTimer = 0.0f;
	}
}

void UGhostRecorderComponent::StartRecording()
{
	if (bIsRecording)
	{
		return;
	}

	bIsRecording = true;
	RecordTimer = 0.0f;
	CurrentRun = FGhostRun();
	CurrentRun.LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld());

	// Record the first sample immediately
	RecordSample();

	UE_LOG(LogTemp, Log, TEXT("GhostRecorder: Started recording on %s"), *CurrentRun.LevelName);
}

void UGhostRecorderComponent::StopRecording()
{
	if (!bIsRecording)
	{
		return;
	}

	bIsRecording = false;

	// Record one final sample
	RecordSample();

	// Set the final time from the last sample
	if (CurrentRun.Samples.Num() > 0)
	{
		CurrentRun.FinalTime = CurrentRun.Samples.Last().Time;
	}

	UE_LOG(LogTemp, Log, TEXT("GhostRecorder: Stopped recording. %d samples, %.2fs total"),
		CurrentRun.Samples.Num(), CurrentRun.FinalTime);

	// Save if this is the best run
	SaveBestRun();
}

void UGhostRecorderComponent::RecordSample()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FGhostSample Sample;
	Sample.Time = GetWorld()->GetTimeSeconds() -
		(CurrentRun.Samples.Num() > 0 ? CurrentRun.Samples[0].Time : GetWorld()->GetTimeSeconds());
	Sample.Location = Owner->GetActorLocation();
	Sample.Rotation = Owner->GetActorRotation();

	// Get velocity from physics component if available
	UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (RootPrim && RootPrim->IsSimulatingPhysics())
	{
		Sample.Velocity = RootPrim->GetPhysicsLinearVelocity();
	}

	// Fix the time for the very first sample
	if (CurrentRun.Samples.Num() == 0)
	{
		Sample.Time = 0.0f;
	}

	CurrentRun.Samples.Add(Sample);
	LastRecordedLocation = Sample.Location;
	LastRecordedRotation = Sample.Rotation;
}

bool UGhostRecorderComponent::ShouldForceSample() const
{
	AActor* Owner = GetOwner();
	if (!Owner || CurrentRun.Samples.Num() == 0)
	{
		return false;
	}

	FVector CurrentLocation = Owner->GetActorLocation();

	// Force a sample if position drifted significantly from last recorded
	float DistSq = FVector::DistSquared(CurrentLocation, LastRecordedLocation);
	if (DistSq > 10000.0f)  // 100 units of drift
	{
		return true;
	}

	// Force a sample if rotation changed a lot
	float RotDiff = FMath::Abs((Owner->GetActorRotation() - LastRecordedRotation).GetNormalized().Yaw);
	if (RotDiff > 30.0f)
	{
		return true;
	}

	return false;
}

float UGhostRecorderComponent::GetCurrentSampleInterval() const
{
	if (IsOwnerInAir())
	{
		return 1.0f / AirSampleRate;
	}
	return 1.0f / GroundSampleRate;
}

bool UGhostRecorderComponent::IsOwnerInAir() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Line trace down to check if grounded
	FHitResult Hit;
	FVector Start = Owner->GetActorLocation();
	FVector End = Start - FVector(0.0f, 0.0f, 70.0f);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

void UGhostRecorderComponent::SaveBestRun()
{
	if (!CurrentRun.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("GhostRecorder: Run too short to save"));
		return;
	}

	UGhostSaveGame* SaveGame = nullptr;

	// Try to load existing save
	if (UGameplayStatics::DoesSaveGameExist(UGhostSaveGame::GetSlotName(), 0))
	{
		SaveGame = Cast<UGhostSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UGhostSaveGame::GetSlotName(), 0));
	}

	if (!SaveGame)
	{
		SaveGame = Cast<UGhostSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UGhostSaveGame::StaticClass()));
	}

	// Check if this run is better (faster) than the saved one
	FGhostRun* ExistingRun = SaveGame->BestRuns.Find(CurrentRun.LevelName);
	if (ExistingRun && ExistingRun->IsValid() && ExistingRun->FinalTime <= CurrentRun.FinalTime)
	{
		UE_LOG(LogTemp, Log, TEXT("GhostRecorder: Existing run (%.2fs) is faster than current (%.2fs), not saving"),
			ExistingRun->FinalTime, CurrentRun.FinalTime);
		return;
	}

	// Save the new best run
	SaveGame->BestRuns.Add(CurrentRun.LevelName, CurrentRun);
	UGameplayStatics::SaveGameToSlot(SaveGame, UGhostSaveGame::GetSlotName(), 0);

	UE_LOG(LogTemp, Log, TEXT("GhostRecorder: Saved new best run (%.2fs, %d samples) for %s"),
		CurrentRun.FinalTime, CurrentRun.Samples.Num(), *CurrentRun.LevelName);
}
