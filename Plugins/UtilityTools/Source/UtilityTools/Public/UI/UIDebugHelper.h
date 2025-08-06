// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#pragma once

DECLARE_LOG_CATEGORY_CLASS(LogUIDebug, Log, All);

namespace UIDebug
{
	static void Print(const FString& Msg, int32 InKey = -1, const FColor& InColor = FColor::MakeRandomColor())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, 5.f, InColor, Msg);

			UE_LOG(LogUIDebug, Warning, TEXT("%s"), *Msg);
		}
	}
}