#pragma once
#include "CoreMinimal.h"
class IInputProcessor;
class UGameInstance;
namespace SimCopterCursorIdle
{
constexpr double TimeoutSeconds = 10.0;
inline bool HasTimedOut(double Now, double LastActivity) { return Now - LastActivity >= TimeoutSeconds; }
SIMCOPTERREMAKE_API TSharedPtr<IInputProcessor> Register(UGameInstance* Instance);
SIMCOPTERREMAKE_API void Unregister(TSharedPtr<IInputProcessor>& Guard);
}
