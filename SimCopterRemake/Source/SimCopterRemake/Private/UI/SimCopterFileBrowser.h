#pragma once
#include "CoreMinimal.h"

class SWidget;

// Runtime native picker shared by New User Game and both Open Game screens.
namespace SimCopterFileBrowser
{
bool Open(TSharedRef<SWidget> Parent, bool bSavedGame, const FString& Title, FString& OutPath);
}
