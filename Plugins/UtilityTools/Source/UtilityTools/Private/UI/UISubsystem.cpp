// Copyright (c) 2025 YaoZhoubo. All rights reserved.


#include "UI/UISubsystem.h"

#include "UI/Widgets/Widget_PrimaryLayout.h"
#include "UI/UIDebugHelper.h"
#include "UI/Widgets/Widget_ActivatableBase.h"
#include "Engine/AssetManager.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UUISubsystem* UUISubsystem::Get(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		return UGameInstance::GetSubsystem<UUISubsystem>(World->GetGameInstance());
	}

	return nullptr;
}

bool UUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!CastChecked<UGameInstance>(Outer)->IsDedicatedServerInstance())
	{
		TArray<UClass*> FoundedClasses;
		GetDerivedClasses(GetClass(), FoundedClasses);

		return FoundedClasses.IsEmpty();
	}

	return false;
}

void UUISubsystem::RegisterCreatedPrimaryLayoutWidget(UWidget_PrimaryLayout* InCreatedWidget)
{
	check(InCreatedWidget);

	CreatedPrimaryLayout = InCreatedWidget;

	UIDebug::Print(TEXT("Register PrimaryLayoutWidget"));
}

void UUISubsystem::PushSoftWidgetToStackAsync(
	const FGameplayTag& InWidgetStackTag, 
	TSoftClassPtr<UWidget_ActivatableBase> InSoftWidgetClass,
	TFunction<void(EAsyncPushWidgetState, UWidget_ActivatableBase*)> AsyncPushStateCallback)
{
	check(!InSoftWidgetClass.IsNull());

	// 异步加载软引用
	UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		InSoftWidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda(
			[InSoftWidgetClass, this, InWidgetStackTag, AsyncPushStateCallback]()
			{
				UClass* LoadedWidgetClass = InSoftWidgetClass.Get();

				check(CreatedPrimaryLayout && LoadedWidgetClass);

				UCommonActivatableWidgetContainerBase* FoundedWidgetStack = CreatedPrimaryLayout->FindWidgetStackByTag(InWidgetStackTag);
				UWidget_ActivatableBase* CreatedWidget = FoundedWidgetStack->AddWidget<UWidget_ActivatableBase>(
					LoadedWidgetClass,
					[AsyncPushStateCallback](UWidget_ActivatableBase& CreatedWidgetInstance)
					{
						AsyncPushStateCallback(EAsyncPushWidgetState::BeforePush, &CreatedWidgetInstance);
					}
				);

				AsyncPushStateCallback(EAsyncPushWidgetState::AfterPush, CreatedWidget);
			}
		));
}
