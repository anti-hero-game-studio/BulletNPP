// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using UnrealBuildTool;

// Bullet third-party build + link module
public class BulletPhysicsEngineLibrary : ModuleRules
{
	private bool BuildBullet(string BuildType)
	{
		// Paths
		string ThirdPartyBulletPath = Path.Combine(ModuleDirectory, "bullet3");         // Bullet source root (CMakeLists.txt)
		string ModulePath           = ModuleDirectory;                                  // Working dir for process
		string BuildRootDir         = BulletBuildUtils.GetBulletBuildRootDir(ModuleDirectory, Target.Platform); // e.g. .../lib/Win64/_build
		string ConfigName           = BulletBuildUtils.GetBuildType(BuildType);         // Debug / Release / RelWithDebInfo
		string LibOutputPath        = BulletBuildUtils.GetBulletLibOutputDir(ModuleDirectory, Target.Platform, BuildType); // .../lib/Win64/Release

		Console.WriteLine("Bullet thirdparty directory: " + ThirdPartyBulletPath);
		Console.WriteLine("Bullet build root: " + BuildRootDir);
		Console.WriteLine("Bullet output libs: " + LibOutputPath);
		Console.WriteLine("Bullet config: " + ConfigName);

		Directory.CreateDirectory(BuildRootDir);
		Directory.CreateDirectory(LibOutputPath);

		// CMake configure options
		var cmakeOptions = "";
		cmakeOptions += " -DUSE_DOUBLE_PRECISION=1";
		cmakeOptions += " -DINSTALL_LIBS=0";
		cmakeOptions += " -DINSTALL_EXTRA_LIBS=0";

		// IMPORTANT: Multi-config generators (Visual Studio) ignore CMAKE_BUILD_TYPE.
		// We still set it for single-config generators, and we ALSO set per-config output dirs.
		cmakeOptions += " -DCMAKE_BUILD_TYPE=" + ConfigName;

		// Ensure static libs land exactly where UBT will link from.
		// For static libs, the correct variable is CMAKE_ARCHIVE_OUTPUT_DIRECTORY (+ per-config variants).
		cmakeOptions += " -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=\"" + LibOutputPath + "\"";
		cmakeOptions += " -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=\"" + LibOutputPath + "\"";
		cmakeOptions += " -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=\"" + LibOutputPath + "\"";

		// Per-config overrides (critical on Windows / multi-config generators).
		string cfgUpper = ConfigName.ToUpperInvariant(); // RELEASE / DEBUG / RELWITHDEBINFO
		cmakeOptions += " -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_" + cfgUpper + "=\"" + LibOutputPath + "\"";
		cmakeOptions += " -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_" + cfgUpper + "=\"" + LibOutputPath + "\"";
		cmakeOptions += " -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_" + cfgUpper + "=\"" + LibOutputPath + "\"";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Force generator + architecture so CMake doesn't guess incorrectly
			cmakeOptions = " -G \"Visual Studio 17 2022\" -A x64 " + cmakeOptions;
			// Prefer DLL runtime; align with UE defaults (commonly /MD).
			cmakeOptions += " -DUSE_MSVC_RUNTIME_LIBRARY_DLL=1";
			cmakeOptions += " -DUSE_MSVC_RELEASE_RUNTIME_ALWAYS=1";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			cmakeOptions += " -DCMAKE_POSITION_INDEPENDENT_CODE=1";
			cmakeOptions += " -DBUILD_SHARED_LIBS=0";
			cmakeOptions += " -DCMAKE_CXX_COMPILER=/usr/bin/clang++";
			cmakeOptions += " -DCMAKE_C_COMPILER=/usr/bin/clang";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// Add mac-specific options here if needed
		}
		else
		{
			Console.WriteLine("[ERROR] Unsupported platform for Bullet build: " + Target.Platform);
			return false;
		}

		// Configure (generate project files)
		var generateCommand = "";
		generateCommand += BulletBuildUtils.GetCMakeExe() + " ";
		generateCommand += " -S\"" + ThirdPartyBulletPath + "\" ";
		generateCommand += " -B\"" + BuildRootDir + "\" ";
		generateCommand += cmakeOptions;

		var configureCode = BulletBuildUtils.ExecuteCommandSync(generateCommand, Path.GetFullPath(ModulePath));
		if (configureCode != 0)
		{
			Console.WriteLine("Bullet CMake configure failed with code: " + configureCode);
			return false;
		}

		// Build
		var buildCommand = "";
		buildCommand += BulletBuildUtils.GetCMakeExe() + " ";
		buildCommand += " --build \"" + BuildRootDir + "\" ";

		// Critical on Win64 (multi-config): explicitly select config
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			buildCommand += " --config " + ConfigName + " ";
		}

		buildCommand += " --target ";
		string[] libraryNames = { "BulletCollision", "BulletDynamics", "LinearMath" };
		foreach (string libraryName in libraryNames)
		{
			buildCommand += libraryName + " ";
		}

		buildCommand += " -j " + Environment.ProcessorCount + " ";

		var buildExitCode = BulletBuildUtils.ExecuteCommandSync(buildCommand, Path.GetFullPath(ModulePath));
		if (buildExitCode != 0)
		{
			Console.WriteLine("Bullet build failed with code: " + buildExitCode);
			return false;
		}

		// Sanity check: ensure expected libs exist where UBT will link them from
		string libExt = (Target.Platform == UnrealTargetPlatform.Win64) ? ".lib" : ".a";
		string prefix = (Target.Platform == UnrealTargetPlatform.Win64) ? "" : "lib";

		foreach (string libraryName in libraryNames)
		{
			string libPath = Path.Combine(LibOutputPath, prefix + libraryName + libExt);
			if (!File.Exists(libPath))
			{
				Console.WriteLine("[ERROR] Expected Bullet lib not found: " + libPath);
				return false;
			}
		}

		return true;
	}

	public BulletPhysicsEngineLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = Target.Configuration == UnrealTargetConfiguration.Debug ||
		              Target.Configuration == UnrealTargetConfiguration.DebugGame;
		bool bDevelopment = Target.Configuration == UnrealTargetConfiguration.Development;

		string BuildFolder = "";
		string BuildSuffix = "";

		if (bDebug)
		{
			BuildFolder = "Debug";
			BuildSuffix = "_Debug"; // NOTE: If your Bullet build does not append suffixes, keep this empty.
			BuildBullet("Debug");
		}
		else if (bDevelopment)
		{
			// Keep RelWithDebInfo path for later, as requested:
			/*
			BuildSuffix = "_RelWithDebInfo";
			BuildFolder = "RelWithDebInfo";
			BuildBullet("Development");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// Some generators place libs under nested config folders; this file avoids that by forcing output dirs.
				BuildFolder = "RelWithDebInfo";
				BuildSuffix = "";
			}
			*/

			// For now, test pure Release performance:
			BuildFolder = "Release";
			BuildSuffix = "";
			BuildBullet("Release");
		}
		else
		{
			BuildFolder = "Release";
			BuildSuffix = "";
			BuildBullet("Release");
		}

		string BuildPlatform = "Win64";
		string LibExtension = ".lib";
		string BuildPrefix = "";

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			BuildPlatform = "Linux";
			LibExtension = ".a";
			BuildPrefix = "lib";
			BuildSuffix = ""; // Bullet static libs typically do not suffix names on Linux
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			BuildPlatform = "Mac";
			LibExtension = ".a";
			BuildPrefix = "lib";
			BuildSuffix = "";
		}

		// Where this Build.cs will link from (must match LibOutputPath produced by BuildBullet)
		string LibrariesPath = Path.Combine(ModuleDirectory, "lib", BuildPlatform, BuildFolder);

		string[] libraryNames = { "BulletCollision", "BulletDynamics", "LinearMath" };
		foreach (string libraryName in libraryNames)
		{
			PublicAdditionalLibraries.Add(
				Path.Combine(LibrariesPath, BuildPrefix + libraryName + BuildSuffix + LibExtension)
			);
		}

		// Include path (Bullet uses mixed headers + sources)
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "bullet3", "src"));
		PublicDefinitions.Add("WITH_BULLET_BINDING=1");
	}
}

// Helpers (kept internal + uniquely named to avoid rules-assembly collisions)
internal static class BulletBuildUtils
{
	public static Tuple<string, string> GetExecuteCommandSync()
	{
		string cmd = "";
		string options = "";

		if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64
#if !UE_5_0_OR_LATER
			|| BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32
#endif
		)
		{
			cmd = "cmd.exe";
			options = "/c ";
		}
		else if (IsUnixPlatform(BuildHostPlatform.Current.Platform))
		{
			cmd = "bash";
			options = "-c ";
		}

		return Tuple.Create(cmd, options);
	}

	public static int ExecuteCommandSync(string Command, string WorkingDir)
	{
		var cmdInfo = GetExecuteCommandSync();

		if (IsUnixPlatform(BuildHostPlatform.Current.Platform))
		{
			Command = " \"" + Command.Replace("\"", "\\\"") + " \"";
		}

		Console.WriteLine("Calling: " + cmdInfo.Item1 + " " + cmdInfo.Item2 + Command);

		var processInfo = new ProcessStartInfo(cmdInfo.Item1, cmdInfo.Item2 + Command)
		{
			CreateNoWindow = true,
			UseShellExecute = false,
			RedirectStandardError = true,
			RedirectStandardOutput = true,
			WorkingDirectory = WorkingDir
		};

		StringBuilder outputString = new StringBuilder();
		using (Process p = Process.Start(processInfo))
		{
			p.OutputDataReceived += (sender, args) => { if (args.Data != null) { outputString.AppendLine(args.Data); Console.WriteLine(args.Data); } };
			p.ErrorDataReceived += (sender, args) => { if (args.Data != null) { outputString.AppendLine(args.Data); Console.WriteLine(args.Data); } };
			p.BeginOutputReadLine();
			p.BeginErrorReadLine();
			p.WaitForExit();

			if (p.ExitCode != 0)
			{
				Console.WriteLine(outputString.ToString());
			}

			return p.ExitCode;
		}
	}

	private static bool IsUnixPlatform(UnrealTargetPlatform Platform)
	{
		return Platform == UnrealTargetPlatform.Linux || Platform == UnrealTargetPlatform.Mac;
	}

	public static string GetCMakeExe()
	{
		string program = "cmake";
		if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64
#if !UE_5_0_OR_LATER
			|| BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32
#endif
		)
		{
			program += ".exe";
		}
		return program;
	}

	// Build root contains CMake cache + generated project files; keep it separate from output libs.
	public static string GetBulletBuildRootDir(string ModuleDirectory, UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.Win64)
		{
			return Path.Combine(ModuleDirectory, "lib", "Win64", "_build");
		}
		else if (Platform == UnrealTargetPlatform.Mac)
		{
			return Path.Combine(ModuleDirectory, "lib", "Mac", "_build");
		}
		else if (Platform == UnrealTargetPlatform.Linux)
		{
			return Path.Combine(ModuleDirectory, "lib", "Linux", "_build");
		}
		return Path.Combine(ModuleDirectory, "lib", "Unknown", "_build");
	}

	// Output directory that UBT will link from: .../lib/<Platform>/<Config>
	public static string GetBulletLibOutputDir(string ModuleDirectory, UnrealTargetPlatform Platform, string BuildType)
	{
		string plat =
			(Platform == UnrealTargetPlatform.Win64) ? "Win64" :
			(Platform == UnrealTargetPlatform.Linux) ? "Linux" :
			(Platform == UnrealTargetPlatform.Mac)   ? "Mac"   :
			"Unknown";

		string configFolder = GetBuildType(BuildType); // Debug/Release/RelWithDebInfo
		return Path.Combine(ModuleDirectory, "lib", plat, configFolder);
	}

	// Maps your BuildType token to a CMake config name
	public static string GetBuildType(string BuildType)
	{
		switch (BuildType)
		{
			case "Debug":
				return "Debug";
			case "Development":
				return "RelWithDebInfo";
			case "Release":
				return "Release";
		}
		return "Release";
	}
}
