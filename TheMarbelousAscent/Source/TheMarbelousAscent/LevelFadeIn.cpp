#include "LevelFadeIn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

bool ULevelFadeIn::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void ULevelFadeIn::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	APlayerController* PC = UGameplayStatics::GetPlayerController(&InWorld, 0);
	if (!PC)
	{
		return;
	}

	UClass* FadeClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Widgets/WBP_Fade.WBP_Fade_C"));
	if (!FadeClass)
	{
		return;
	}

	if (UUserWidget* Fade = CreateWidget<UUserWidget>(PC, FadeClass))
	{
		Fade->AddToViewport(100);
	}
}
