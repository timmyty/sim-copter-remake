#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/SimCopterSettings.h"
#include "UI/SimCopterHangarArt.h"
#include "UI/SSimCopterCareerSelect.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"

class FSimCopterCareerPreviewCommand : public IAutomationLatentCommand
{
public:
	explicit FSimCopterCareerPreviewCommand(FAutomationTestBase* InTest):Test(InTest){}

	virtual bool Update() override
	{
		if(!Instance)
		{
			Instance=NewObject<UGameInstance>(GEngine);Instance->InitializeStandalone();
			Instance->GetWorld()->InitializeActorsForPlay(FURL());
			Art=NewObject<USimCopterHangarArt>(Instance);Art->AddToRoot();Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
			Settings=Instance->GetSubsystem<USimCopterSettings>();
			OldPreference=Settings->WantsRemakeFeatures();OldActive=Settings->AreRemakeFeaturesEnabled();
			Open();
		}
		if(FPlatformTime::Seconds()-Started>25)
		{
			Test->AddError(FString::Printf(TEXT("Career preview did not advance/loop on visit %d (extra features %s)"),Visit,Visit==2?TEXT("on"):TEXT("off")));
			return Finish();
		}
		for(int32 I=0;I<3;++I)
		{
			if(!Players[I] || !Players[I]->IsPlaying()) return false;
			const double Time=Players[I]->GetTime().GetTotalSeconds();
			if(PreviousTime[I]>4 && Time<1) Looped[I]=true;
			PreviousTime[I]=Time;
			if(First[I].IsEmpty() && Time>.7) First[I]=Pixels(I);
			else if(!First[I].IsEmpty() && !Moved[I] && Time>1.8)
			{
				const auto Next=Pixels(I);int32 Changed=0;
				for(int32 P=0;P<Next.Num();++P)
					Changed+=FMath::Abs(int32(Next[P].R)-First[I][P].R)+FMath::Abs(int32(Next[P].G)-First[I][P].G)+FMath::Abs(int32(Next[P].B)-First[I][P].B)>30;
				Test->TestTrue(TEXT("Decoded rotating map changes pixels"),Changed>500);
				Test->TestTrue(TEXT("Preview loops"),Players[I]->IsLooping());Moved[I]=true;
			}
		}
		for(int32 I=0;I<3;++I) if(!Moved[I] || !Looped[I]) return false;
		Test->AddInfo(FString::Printf(TEXT("Visit %d: all three previews decoded changing frames and looped; extra features %s"),Visit,Visit==2?TEXT("on"):TEXT("off")));
		Art->StopCareerCityMovies();Page.Reset();
		if(++Visit==4) return Finish();
		Open();return false;
	}
private:
	void Open()
	{
		Settings->SetRemakeFeaturesPreference(Visit==2);Settings->ApplyRemakeFeaturesForSession();
		Page=SNew(SSimCopterCareerSelect).Art(Art).Cities(TArray<int32>{0,1,2}).AllowCancel(true);
		for(int32 I=0;I<3;++I)
		{
			Brushes[I]=Art->GetCareerCityMovieBrush(I);
			auto* Texture=Brushes[I]?Cast<UMediaTexture>(Brushes[I]->GetResourceObject()):nullptr;
			Players[I]=Texture?Texture->GetMediaPlayer():nullptr;
			Test->TestNotNull(TEXT("First and repeated career selections have a media player in either mode"),Players[I]);
			First[I].Reset();Moved[I]=Looped[I]=false;PreviousTime[I]=0;
		}
		Started=FPlatformTime::Seconds();
	}
	TArray<FColor> Pixels(int32 City)
	{
		FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(SNew(SImage).Image(Brushes[City]),FVector2D(200,108));
		FlushRenderingCommands();TArray<FColor> Result;Target->GameThread_GetRenderTargetResource()->ReadPixels(Result);
		for(auto& P:Result)P.A=255;
		const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/fog-engines-v123/previews");
		IFileManager::Get().MakeDirectory(*Folder,true);
		FImageUtils::SaveImageByExtension(*(Folder/FString::Printf(TEXT("visit%d-city%d-%s.png"),Visit,City,First[City].IsEmpty()?TEXT("first"):TEXT("next"))),FImageView(Result.GetData(),200,108));
		return Result;
	}
	bool Finish()
	{
		Page.Reset();Art->StopCareerCityMovies();Art->RemoveFromRoot();
		Settings->SetRemakeFeaturesPreference(OldActive);Settings->ApplyRemakeFeaturesForSession();
		Settings->SetRemakeFeaturesPreference(OldPreference);Settings->SaveConfig();
		auto* World=Instance->GetWorld();Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
	}
	FAutomationTestBase* Test;
	UGameInstance* Instance=nullptr;USimCopterHangarArt* Art=nullptr;USimCopterSettings* Settings=nullptr;
	TSharedPtr<SSimCopterCareerSelect> Page;
	const FSlateBrush* Brushes[3]={};UMediaPlayer* Players[3]={};TArray<FColor> First[3];
	bool Moved[3]={},Looped[3]={},OldPreference=false,OldActive=false;double PreviousTime[3]={},Started=0;int32 Visit=0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCareerPreviewTest,"SimCopter.FrontEnd.CareerPreviewPlayback",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FSimCopterCareerPreviewTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSimCopterCareerPreviewCommand(this));return true;
}
#endif
