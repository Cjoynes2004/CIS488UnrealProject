#include "RaceResultWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

URaceResultWidget::URaceResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DimOverlay(nullptr)
	, HeadlineText(nullptr)
	, PlayerTimeText(nullptr)
	, GhostTimeText(nullptr)
{
}

bool URaceResultWidget::Initialize()
{
	const bool bResult = Super::Initialize();

	if (!WidgetTree)
	{
		return bResult;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultRoot"));
		WidgetTree->RootWidget = RootCanvas;
	}

	if (!DimOverlay)
	{
		// Full-screen dim background so the result text reads against any scene.
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultDim"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(Background);
		BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BgSlot->SetOffsets(FMargin(0.0f));
		DimOverlay = Background;

		// Vertical stack: headline + two time lines, centered.
		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultStack"));
		Background->SetContent(Stack);
		Background->SetVerticalAlignment(VAlign_Center);
		Background->SetHorizontalAlignment(HAlign_Center);

		HeadlineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultHeadline"));
		HeadlineText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 96));
		HeadlineText->SetJustification(ETextJustify::Center);
		HeadlineText->SetText(FText::FromString(TEXT("--")));
		UVerticalBoxSlot* HeadSlot = Stack->AddChildToVerticalBox(HeadlineText);
		HeadSlot->SetHorizontalAlignment(HAlign_Center);
		HeadSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

		PlayerTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultPlayer"));
		PlayerTimeText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 36));
		PlayerTimeText->SetJustification(ETextJustify::Center);
		PlayerTimeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UVerticalBoxSlot* P1 = Stack->AddChildToVerticalBox(PlayerTimeText);
		P1->SetHorizontalAlignment(HAlign_Center);

		GhostTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultGhost"));
		GhostTimeText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 36));
		GhostTimeText->SetJustification(ETextJustify::Center);
		GhostTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.85f, 1.0f)));
		UVerticalBoxSlot* P2 = Stack->AddChildToVerticalBox(GhostTimeText);
		P2->SetHorizontalAlignment(HAlign_Center);
	}

	return bResult;
}

void URaceResultWidget::ShowResult(bool bPlayerWon, float PlayerSeconds, float GhostSeconds, bool bGhostFinished)
{
	if (HeadlineText)
	{
		HeadlineText->SetText(FText::FromString(bPlayerWon ? TEXT("YOU WIN!") : TEXT("GHOST WINS")));
		HeadlineText->SetColorAndOpacity(FSlateColor(
			bPlayerWon ? FLinearColor(0.2f, 1.0f, 0.4f) : FLinearColor(1.0f, 0.4f, 0.4f)));
	}
	if (PlayerTimeText)
	{
		PlayerTimeText->SetText(FText::FromString(FString::Printf(TEXT("Your time: %.2fs"), PlayerSeconds)));
	}
	if (GhostTimeText)
	{
		if (bGhostFinished)
		{
			GhostTimeText->SetText(FText::FromString(FString::Printf(TEXT("Ghost time: %.2fs"), GhostSeconds)));
			GhostTimeText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			GhostTimeText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
