#include "RaceCountdownWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Styling/SlateBrush.h"

URaceCountdownWidget::URaceCountdownWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DisplayImage(nullptr)
	, OutlineImage(nullptr)
	, FallbackText(nullptr)
	, GoTexture(nullptr)
{
}

namespace
{
	// Try a few path conventions when looking up countdown textures so the
	// designer doesn't have to fight asset-naming exactly.
	UTexture2D* TryLoadTexture(const TArray<FString>& Candidates)
	{
		for (const FString& Path : Candidates)
		{
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path))
			{
				return Tex;
			}
		}
		return nullptr;
	}
}

bool URaceCountdownWidget::Initialize()
{
	const bool bResult = Super::Initialize();

	if (!WidgetTree)
	{
		return bResult;
	}

	// Build a centered, always-on-top layout: a canvas with one image slot and
	// a text fallback stacked behind it.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CountdownRoot"));
		WidgetTree->RootWidget = RootCanvas;
	}

	if (!FallbackText)
	{
		FallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountdownFallback"));
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 200);
		FallbackText->SetFont(Font);
		FallbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.2f)));
		FallbackText->SetJustification(ETextJustify::Center);
		FallbackText->SetText(FText::GetEmpty());
		UCanvasPanelSlot* TextSlot = RootCanvas->AddChildToCanvas(FallbackText);
		TextSlot->SetAnchors(FAnchors(0.5f, 0.35f));
		TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		TextSlot->SetSize(FVector2D(600.0f, 300.0f));
		TextSlot->SetPosition(FVector2D::ZeroVector);
	}

	// Outline image is added BEFORE the display image so it draws underneath.
	// We render the same texture tinted black at a slight offset to fake an
	// outline / drop-shadow that softens the edges and adds readability over
	// any background.
	if (!OutlineImage)
	{
		OutlineImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CountdownOutline"));
		OutlineImage->SetVisibility(ESlateVisibility::Hidden);
		OutlineImage->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		UCanvasPanelSlot* OutlineSlot = RootCanvas->AddChildToCanvas(OutlineImage);
		OutlineSlot->SetAnchors(FAnchors(0.5f, 0.35f));
		OutlineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		OutlineSlot->SetSize(FVector2D(320.0f, 320.0f));
		OutlineSlot->SetPosition(FVector2D(4.0f, 6.0f));
	}

	if (!DisplayImage)
	{
		DisplayImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CountdownImage"));
		DisplayImage->SetVisibility(ESlateVisibility::Hidden);
		UCanvasPanelSlot* ImgSlot = RootCanvas->AddChildToCanvas(DisplayImage);
		ImgSlot->SetAnchors(FAnchors(0.5f, 0.35f));
		ImgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ImgSlot->SetSize(FVector2D(320.0f, 320.0f));
		ImgSlot->SetPosition(FVector2D::ZeroVector);
	}

	// Lazy-load textures. Try a few common naming patterns.
	if (NumberTextures.Num() == 0)
	{
		NumberTextures.SetNum(3);
		NumberTextures[0] = TryLoadTexture({
			TEXT("/Game/UI/Textures/Countdown/One.One"),
			TEXT("/Game/UI/Textures/Countdown/T_Countdown_1.T_Countdown_1"),
			TEXT("/Game/UI/Textures/Countdown/Countdown_1.Countdown_1"),
			TEXT("/Game/UI/Textures/Countdown/1.1")});
		NumberTextures[1] = TryLoadTexture({
			TEXT("/Game/UI/Textures/Countdown/Two.Two"),
			TEXT("/Game/UI/Textures/Countdown/T_Countdown_2.T_Countdown_2"),
			TEXT("/Game/UI/Textures/Countdown/Countdown_2.Countdown_2"),
			TEXT("/Game/UI/Textures/Countdown/2.2")});
		NumberTextures[2] = TryLoadTexture({
			TEXT("/Game/UI/Textures/Countdown/Three.Three"),
			TEXT("/Game/UI/Textures/Countdown/T_Countdown_3.T_Countdown_3"),
			TEXT("/Game/UI/Textures/Countdown/Countdown_3.Countdown_3"),
			TEXT("/Game/UI/Textures/Countdown/3.3")});
	}
	if (!GoTexture)
	{
		GoTexture = TryLoadTexture({
			TEXT("/Game/UI/Textures/Countdown/Go.Go"),
			TEXT("/Game/UI/Textures/Countdown/T_Countdown_Go.T_Countdown_Go"),
			TEXT("/Game/UI/Textures/Countdown/Countdown_Go.Countdown_Go"),
			TEXT("/Game/UI/Textures/Countdown/GO.GO")});
	}

	return bResult;
}

void URaceCountdownWidget::SetTextureOrText(UTexture2D* Texture, const FString& Fallback)
{
	if (Texture && DisplayImage)
	{
		// Preserve the source PNG's aspect ratio. Cap the longest side at
		// MaxSide so a tall numeral and a wide "Go!" both render at a sane
		// screen size without stretching.
		const float SrcW = static_cast<float>(Texture->GetSizeX());
		const float SrcH = static_cast<float>(Texture->GetSizeY());
		const float MaxSide = 280.0f;
		float DstW = SrcW;
		float DstH = SrcH;
		if (SrcW >= SrcH && SrcW > 0.0f)
		{
			DstW = MaxSide;
			DstH = MaxSide * (SrcH / SrcW);
		}
		else if (SrcH > 0.0f)
		{
			DstH = MaxSide;
			DstW = MaxSide * (SrcW / SrcH);
		}
		const FVector2D Size(DstW, DstH);

		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(SrcW, SrcH);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		DisplayImage->SetBrush(Brush);
		DisplayImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* DisplaySlot = Cast<UCanvasPanelSlot>(DisplayImage->Slot))
		{
			DisplaySlot->SetSize(Size);
		}

		// Mirror the texture onto the outline image, tinted black and sized
		// slightly larger and CENTERED (no offset) so it reads as a soft halo
		// surrounding the number from all sides rather than a hard drop
		// shadow on one corner. Combined with a small alpha drop on the
		// foreground, the number's edges fade into the dark halo and look
		// noticeably softer.
		if (OutlineImage)
		{
			FSlateBrush OutlineBrush;
			OutlineBrush.SetResourceObject(Texture);
			OutlineBrush.ImageSize = FVector2D(SrcW, SrcH);
			OutlineBrush.DrawAs = ESlateBrushDrawType::Image;
			OutlineImage->SetBrush(OutlineBrush);
			OutlineImage->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
			OutlineImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* OutlineSlot = Cast<UCanvasPanelSlot>(OutlineImage->Slot))
			{
				const FVector2D Halo = Size + FVector2D(14.0f, 14.0f);
				OutlineSlot->SetSize(Halo);
				// Centered offset so halo grows symmetrically.
				OutlineSlot->SetPosition(-(Halo - Size) * 0.5f);
			}
		}

		// Soften the foreground itself by dropping alpha slightly so its edges
		// blend into the halo behind it.
		DisplayImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.92f));

		if (FallbackText)
		{
			FallbackText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	else if (FallbackText)
	{
		FallbackText->SetText(FText::FromString(Fallback));
		FallbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (DisplayImage)
		{
			DisplayImage->SetVisibility(ESlateVisibility::Hidden);
		}
		if (OutlineImage)
		{
			OutlineImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void URaceCountdownWidget::ShowNumber(int32 Number)
{
	const int32 Idx = FMath::Clamp(Number - 1, 0, NumberTextures.Num() - 1);
	UTexture2D* Tex = NumberTextures.IsValidIndex(Idx) ? NumberTextures[Idx] : nullptr;
	SetTextureOrText(Tex, FString::Printf(TEXT("%d"), Number));
}

void URaceCountdownWidget::ShowGo()
{
	SetTextureOrText(GoTexture, TEXT("GO!"));
}
