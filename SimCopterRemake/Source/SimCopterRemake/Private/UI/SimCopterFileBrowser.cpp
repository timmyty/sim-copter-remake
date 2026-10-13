#include "SimCopterFileBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

bool SimCopterFileBrowser::Open(TSharedRef<SWidget> Parent, bool bSavedGame, const FString& Title, FString& OutPath)
{
	OutPath.Reset();
#if PLATFORM_WINDOWS
	// Comdlg32 is available in Shipping; DesktopPlatform is an editor/developer module.
	TArray<WCHAR> Filename; Filename.SetNumZeroed(32768);
	const FString InitialDir = FPaths::ConvertRelativePathToFull(bSavedGame ? FPaths::ProjectSavedDir() / TEXT("SaveGames") : FString());
	OPENFILENAMEW Dialog = {};
	Dialog.lStructSize = sizeof(Dialog);
	Dialog.hwndOwner = static_cast<HWND>(const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(Parent)));
	Dialog.lpstrFilter = bSavedGame ? L"SimCopter Remake saved games (*.sav)\0*.sav\0\0" : L"SimCity 2000 city files (*.sc2)\0*.sc2\0\0";
	Dialog.lpstrTitle = *Title;
	Dialog.lpstrInitialDir = bSavedGame ? *InitialDir : nullptr;
	Dialog.lpstrFile = Filename.GetData();
	Dialog.nMaxFile = Filename.Num();
	Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	if (!GetOpenFileNameW(&Dialog)) return false;
	OutPath = FPaths::ConvertRelativePathToFull(FString(Filename.GetData()));
	return true;
#else
	FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("FileBrowser", "Windows", "File Explorer browsing is available on Windows."));
	return false;
#endif
}
