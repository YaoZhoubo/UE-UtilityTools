// Copyright (c) 2025 YaoZhoubo. All rights reserved.


#include "UI/AsyncAction_PushSoftWidget.h"

#include "UI/Widgets/Widget_ActivatableBase.h"
#include "UI/UISubsystem.h"

UAsyncAction_PushSoftWidget* UAsyncAction_PushSoftWidget::PushSoftWidget(
	const UObject* WorldContextObject,
	APlayerController* OwningPlayerController, 
	TSoftClassPtr<UWidget_ActivatableBase> InSoftWidgetClass, 
	UPARAM(meta = (Categories = "UI.WidgetStack")) FGameplayTag InWidgetStackTag, 
	bool bFocusOnNewlyPushedWidget /*= true*/)
{
	check(!InSoftWidgetClass.IsNull());

	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			UAsyncAction_PushSoftWidget* Node = NewObject<UAsyncAction_PushSoftWidget>();
			Node->CachedOwningWorld = World;
			Node->CachedOwningPlayerController = OwningPlayerController;
			Node->CachedSoftWidgetClass = InSoftWidgetClass;
			Node->CachedWidgetStackTag = InWidgetStackTag;
			Node->bCachedFocusOnNewlyPushedWidget = bFocusOnNewlyPushedWidget;

			Node->RegisterWithGameInstance(World);

			return Node;
		}
	}

	return nullptr;
}

void UAsyncAction_PushSoftWidget::Activate()
{
	UUISubsystem* UISubsystem = UUISubsystem::Get(CachedOwningWorld.Get());

	UISubsystem->PushSoftWidgetToStackAsync(CachedWidgetStackTag, CachedSoftWidgetClass,
		[this](EAsyncPushWidgetState InPushState, UWidget_ActivatableBase* PushedWidget)
		{
			switch (InPushState)
			{
			case EAsyncPushWidgetState::BeforePush:
				PushedWidget->SetOwningPlayer(CachedOwningPlayerController.Get());
				OnWidgetBeforePush.Broadcast(PushedWidget);
				break;

			case EAsyncPushWidgetState::AfterPush:
				OnWidgetAfterPush.Broadcast(PushedWidget);
				if (bCachedFocusOnNewlyPushedWidget)
				{
					if (UWidget* WidgetToFocus = PushedWidget->GetDesiredFocusTarget())
					{
						WidgetToFocus->SetFocus();
					}
				}
				SetReadyToDestroy();
				break;

			default:
				break;
			}
		}
		);
}
