// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using System.Security.Cryptography;
using EpicGames.Core;
using UnrealBuildTool;

public class SimCopterRemake : ModuleRules
{
	public SimCopterRemake(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Several format readers use same-named helpers in anonymous namespaces; unity builds
		// merge multiple .cpp files into one TU, so chunk reshuffles caused collisions whenever
		// a file was added. Per-file compilation keeps those helpers properly private.
		bUseUnity = false;
		RuntimeDependencies.Add("$(ProjectDir)/Config/FigureAdjustments.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/Config/HighQualityMusic.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/Config/FLAC-LICENSE.txt", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/Briefings/*.wav", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/DriveInVideos/*.mp4", StagedFileType.NonUFS);

		// MeshDescription/StaticMeshDescription back UStaticMesh::BuildFromMeshDescriptions, which the
		// city build uses to turn each distinct GEO building model into a runtime static mesh so
		// buildings can be placed as removable instances instead of baked into one merged mesh.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "MeshDescription", "StaticMeshDescription" });

		// DaySequence and CelestialVault back the level's day/night cycle. The CityRender actor is an
		// ACelestialVaultDaySequenceActor: USimCopterDayNightFogComponent reads
		// GetTimeOfDay()/GetDayLength() off its DaySequence base to drive the height fog, and
		// USimCopterMoonDiscComponent reaches its MoonDiscComponent property to override the moon
		// disc's material brightness. Both plugins are already enabled in the .uproject (DaySequence
		// via CelestialVault's own dependency), so this only adds the link, not a new plugin.
		// ApplicationCore for FDisplayMetrics, which seeds the first run's resolution from the
		// monitor the game opened on; RHI for IsRayTracingEnabled, which decides whether the
		// Settings page may offer Hardware Lumen.
		PrivateDependencyModuleNames.AddRange(new string[] { "VorbisAudioDecoder", "Slate", "SlateCore", "UMG", "RenderCore", "RHI", "ApplicationCore", "MediaAssets", "AudioMixer", "MoviePlayer", "DaySequence", "CelestialVault", "Json", "JsonUtilities" });

		// The Graphics page replaces the original's render.bmp options with Unreal's, so it drives
		// NVIDIA's blueprint libraries directly rather than poking console variables. All three
		// plugins are already required by the .uproject; they publish WITH_DLSS / WITH_STREAMLINE
		// as 0 off Windows, so the call sites stay guarded and this stays Windows-only.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libFLAC");
			PublicSystemLibraries.Add("Comdlg32.lib");
			PrivateDependencyModuleNames.AddRange(new string[] { "DLSSBlueprint", "StreamlineDLSSGBlueprint", "StreamlineReflexBlueprint" });
		}
		else
		{
			// The .uproject allow-lists those plugins to Win64, so elsewhere nothing publishes the
			// guards and Clang's -Wundef rejects the bare #if. Define them off explicitly.
			PrivateDefinitions.Add("WITH_DLSS=0");
			PrivateDefinitions.Add("WITH_STREAMLINE=0");
		}

		// A packaged game reads the original data as loose files beside the executable. UAT cannot
		// stage files directly from the repo-level Reference folder, so game targets first copy the
		// six runtime data trees into the gitignored project Intermediate folder. DefaultGame.ini's
		// [Staging] remap then lifts them to <Package>/SimCopter, which is ../SimCopter from the
		// packaged ProjectDir and therefore the first root SimCopterOriginalGamePaths searches.
		// Editor targets deliberately skip this 237 MB copy; they read Reference directly.
		if (Target.Type == TargetType.Game)
		{
			// Stage only verified compressed KINV recordings, never legacy WAV masters.
			string KinvConfig = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Config", "KinvMusic.json"));
			JsonObject Kinv = JsonObject.Read(new FileReference(KinvConfig));
			JsonObject[] KinvTracks = Kinv.GetObjectArrayField("tracks");
			if (KinvTracks.Length != 21) throw new BuildException("KINV requires all 21 compressed tracks.");
			foreach (JsonObject Track in KinvTracks)
			{
				string Name = Track.GetStringField("file");
				if (Name != Path.GetFileName(Name) || !Name.EndsWith(".ogg")) throw new BuildException("Invalid KINV file: {0}", Name);
				string File = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Content", "Radio", "InvokeTheRevoked", Name));
				if (!System.IO.File.Exists(File) || FileSha256(File) != Track.GetStringField("sha256"))
					throw new BuildException("Missing or changed KINV track: {0}. Run Tools/Unreal/PrepareKinvMusic.py with the masters folder.", Name);
				RuntimeDependencies.Add(File, StagedFileType.NonUFS);
			}
			for (int IntroIndex = 1; IntroIndex <= 2; ++IntroIndex)
			{
				string IntroMovie = Path.GetFullPath(Path.Combine(ModuleDirectory,
					"..", "..", "Content", "Generated", "Movies", "Intro", $"INTRO{IntroIndex}.mp4"));
				if (!File.Exists(IntroMovie))
				{
					throw new BuildException("Missing original intro movie. Run python Tools/Unreal/BakeIntroMovies.py before packaging.");
				}
				RuntimeDependencies.Add(IntroMovie, StagedFileType.NonUFS);
			}
			string LoadingAtlas = Path.GetFullPath(Path.Combine(ModuleDirectory,
				"..", "..", "Content", "Generated", "Loading", "HRGLASS.png"));
			if (!File.Exists(LoadingAtlas))
			{
				throw new BuildException("Missing original loading animation. Run python Tools/Unreal/BakeLoadingScreen.py before packaging.");
			}
			string OriginalGameRoot = Path.GetFullPath(Path.Combine(
				ModuleDirectory, "..", "..", "..", "Reference", "SimCopterOriginalGame"));
			string StagingRoot = Path.Combine("$(ProjectDir)", "Intermediate", "OriginalGameStaging");
			var ReplacedMusic = new Dictionary<string, string>(System.StringComparer.OrdinalIgnoreCase);
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				string MusicManifest = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Config", "HighQualityMusic.json"));
				JsonObject Music = JsonObject.Read(new FileReference(MusicManifest));
				foreach (JsonObject Track in Music.GetObjectArrayField("tracks"))
				{
					string RelativeFlac = Track.GetStringField("file");
					if (!RelativeFlac.StartsWith("music-hq/") || RelativeFlac.Contains("..") || RelativeFlac.Contains('\\'))
						throw new BuildException("Invalid shared soundtrack path: {0}", RelativeFlac);
					string Flac = Path.Combine(OriginalGameRoot, "sound", RelativeFlac);
					if (!File.Exists(Flac) || FileSha256(Flac) != Track.GetStringField("sha256"))
						throw new BuildException("Missing or changed verified FLAC: {0}. Run Tools/Unreal/PrepareHighQualityMusic.py with the supplied soundtrack folder.", Flac);
					foreach (JsonObject Original in Track.GetObjectArrayField("originals"))
						ReplacedMusic.Add("sound/" + Original.GetStringField("path"), Original.GetStringField("sha256"));
				}
			}
			for (int CityIndex = 0; CityIndex < 30; ++CityIndex)
			{
				if (!File.Exists(Path.Combine(OriginalGameRoot, "sound", "cityride", $"city{CityIndex}.wav")))
				{
					throw new BuildException("Missing original cityride audio. Run python Tools/Unreal/BakeCityIntroAudio.py before packaging.");
				}
			}
			string[] RequiredDirectories = { "bmp", "cities", "geo", "sound", "tweak", "x" };
			string[] RequiredFiles =
			{
				"bmp/sim3d.bmp",
				"cities/career/city0.sc2",
				"geo/sim3d1.max",
				"sound/coploop2.wav",
				"tweak/career.twk",
				"x/people.df",
				"x/privanim.df",
			};

			foreach (string DirectoryName in RequiredDirectories)
			{
				string SourceDirectory = Path.Combine(OriginalGameRoot, DirectoryName);
				if (!Directory.Exists(SourceDirectory))
				{
					throw new BuildException(
						"Cannot build a playable SimCopterRemake game target: required original-game " +
						"directory '{0}' is missing. Restore Reference/SimCopterOriginalGame/{1}.",
						SourceDirectory, DirectoryName);
				}

				// Copy original game files to Intermediate staging tree during target setup
				// so that produced staged items exist on disk when UBT validates dependencies.
				string ResolvedStagingRoot = Path.GetFullPath(Path.Combine(
					ModuleDirectory, "..", "..", "Intermediate", "OriginalGameStaging"));
				string[] AllFiles = Directory.GetFiles(SourceDirectory, "*", SearchOption.AllDirectories);
				foreach (string SourceFilePath in AllFiles)
				{
					if (DirectoryName == "tweak" && !SourceFilePath.EndsWith(".twk", System.StringComparison.OrdinalIgnoreCase))
					{
						continue;
					}

					string RelativePath = Path.GetRelativePath(OriginalGameRoot, SourceFilePath);
					// Only omit byte-verified originals. Custom songs with familiar names remain WAVs;
					// the runtime applies the same protection. Genre and mix share one music-hq file.
					if (ReplacedMusic.TryGetValue(RelativePath.Replace('\\', '/'), out string OriginalHash) &&
						FileSha256(SourceFilePath) == OriginalHash) continue;
					if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) &&
						RelativePath.Replace('\\', '/').StartsWith("sound/music-hq/")) continue;
					string TargetDiskPath = Path.Combine(ResolvedStagingRoot, RelativePath);
					string StagedManifestPath = Path.Combine(StagingRoot, RelativePath);

					Directory.CreateDirectory(Path.GetDirectoryName(TargetDiskPath));
					if (!File.Exists(TargetDiskPath) || File.GetLastWriteTimeUtc(SourceFilePath) > File.GetLastWriteTimeUtc(TargetDiskPath))
					{
						File.Copy(SourceFilePath, TargetDiskPath, true);
					}

					RuntimeDependencies.Add(StagedManifestPath, StagedFileType.NonUFS);
				}
			}

			foreach (string RelativeFile in RequiredFiles)
			{
				string SourceFile = Path.Combine(OriginalGameRoot, RelativeFile);
				if (!File.Exists(SourceFile))
				{
					throw new BuildException(
						"Cannot build a playable SimCopterRemake game target: required original-game " +
						"file '{0}' is missing. Restore Reference/SimCopterOriginalGame/{1}.",
						SourceFile, RelativeFile);
				}
			}
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}

	private static string FileSha256(string FileName)
	{
		using SHA256 Hash = SHA256.Create();
		using FileStream Stream = File.OpenRead(FileName);
		return System.Convert.ToHexString(Hash.ComputeHash(Stream)).ToLowerInvariant();
	}
}
