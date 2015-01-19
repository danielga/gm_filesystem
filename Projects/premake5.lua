SDK_FOLDER = "E:/Programming/source-sdk-2013/mp/src"
GARRYSMOD_INCLUDE_FOLDER = "../gmod-module-base/include"
SOURCE_FOLDER = "../Source"
PROJECT_FOLDER = os.get() .. "/" .. _ACTION

solution("gm_filesystem")
	language("C++")
	location(PROJECT_FOLDER)
	flags({"NoPCH", "StaticRuntime"})
	platforms({"x86"})
	configurations({"Release", "Debug"})

	filter("platforms:x86")
		architecture("x32")

	filter("configurations:Release")
		optimize("On")
		vectorextensions("SSE2")
		objdir(PROJECT_FOLDER .. "/Intermediate")
		targetdir(PROJECT_FOLDER .. "/Release")

	filter("configurations:Debug")
		flags({"Symbols"})
		vectorextensions("SSE2")
		objdir(PROJECT_FOLDER .. "/Intermediate")
		targetdir(PROJECT_FOLDER .. "/Debug")

	project("gmsv_filesystem")
		kind("SharedLib")
		defines({"GMMODULE", "GAME_DLL", "FILESYSTEM_SERVER"})
		includedirs({
			SOURCE_FOLDER,
			GARRYSMOD_INCLUDE_FOLDER,
			SDK_FOLDER .. "/public",
			SDK_FOLDER .. "/public/tier0",
			SDK_FOLDER .. "/public/tier1"
		})
		files({SOURCE_FOLDER .. "/main.cpp"})
		vpaths({["Sources"] = SOURCE_FOLDER .. "/**.cpp"})
		links({"tier0", "tier1"})

		targetprefix("")
		targetextension(".dll")

		filter("system:windows")
			libdirs({SDK_FOLDER .. "/lib/public"})
			targetsuffix("_win32")

		filter("system:linux")
			defines({"POSIX", "GNUC", "_LINUX"})
			libdirs({SDK_FOLDER .. "/lib/public/linux32"})
			links({"dl"})
			buildoptions({"-std=c++11"})
			targetsuffix("_linux")

			prelinkcommands({"ln -sf " .. SDK_FOLDER .. "/lib/public/linux32/tier1.a " .. SDK_FOLDER .. "/lib/public/linux32/libtier1.a"})

		filter("system:macosx")
			libdirs({SDK_FOLDER .. "/lib/public/osx32"})
			links({"dl"})
			buildoptions({"-std=c++11"})
			targetsuffix("_mac")

	project("gmcl_filesystem")
		kind("SharedLib")
		defines({"GMMODULE", "CLIENT_DLL", "FILESYSTEM_CLIENT"})
		includedirs({
			SOURCE_FOLDER,
			GARRYSMOD_INCLUDE_FOLDER,
			SDK_FOLDER .. "/public",
			SDK_FOLDER .. "/public/tier0",
			SDK_FOLDER .. "/public/tier1"
		})
		files({SOURCE_FOLDER .. "/main.cpp"})
		vpaths({["Sources"] = SOURCE_FOLDER .. "/**.cpp"})
		links({"tier0", "tier1"})

		targetprefix("")
		targetextension(".dll")

		filter("system:windows")
			libdirs({SDK_FOLDER .. "/lib/public"})
			targetsuffix("_win32")

		filter("system:linux")
			defines({"POSIX", "GNUC", "_LINUX"})
			libdirs({SDK_FOLDER .. "/lib/public/linux32"})
			links({"dl"})
			buildoptions({"-std=c++11"})
			targetsuffix("_linux")

			prelinkcommands({"ln -sf " .. SDK_FOLDER .. "/lib/public/linux32/tier1.a " .. SDK_FOLDER .. "/lib/public/linux32/libtier1.a"})

		filter("system:macosx")
			libdirs({SDK_FOLDER .. "/lib/public/osx32"})
			links({"dl"})
			buildoptions({"-std=c++11"})
			targetsuffix("_mac")