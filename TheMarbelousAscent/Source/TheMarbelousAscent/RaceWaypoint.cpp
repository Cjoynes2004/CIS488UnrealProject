#include "RaceWaypoint.h"
#include "Components/SphereComponent.h"
#include "Components/BillboardComponent.h"

ARaceWaypoint::ARaceWaypoint()
{
	PrimaryActorTick.bCanEverTick = false;

	WaypointIndex = 0;
	AcceptanceRadius = 200.0f;
	bRequiresJump = false;
	SpeedMultiplier = 1.0f;
	bShouldBrake = false;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetSphereRadius(AcceptanceRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerSphere->SetHiddenInGame(true);
	RootComponent = TriggerSphere;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->SetupAttachment(RootComponent);
	}
#endif
}

void ARaceWaypoint::BeginPlay()
{
	Super::BeginPlay();
}

#if WITH_EDITOR
void ARaceWaypoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ARaceWaypoint, AcceptanceRadius))
	{
		if (TriggerSphere)
		{
			TriggerSphere->SetSphereRadius(AcceptanceRadius);
		}
	}
}
#endif
