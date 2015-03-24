SDK_FOLDER = "E:/Programming/source-sdk-2013/mp/src"
GARRYSMOD_MODULE_BASE_FOLDER = "../gmod-module-base"
SCANNING_FOLDER = "../scanning"
SOURCE_FOLDER = "../source"
PROJECT_FOLDER = os.get() .. "/" .. _ACTION

solution("gm_filesystem")
	language("C++")
	location(PROJECT_FOLDER)
	warnings("Extra")
	flags({"NoPCH", "StaticRuntime"})
	platforms({"x86"})
	configurations({"Release", "Debug"})

	filter("platforms:x86")
		architecture("x32")

	filter("configurations:Release")
		optimize("On")
		vectorextensions("SSE2")
		objdir(PROJECT_FOLDER .. "/intermediate")
		targetdir(PROJECT_FOLDER .. "/release")

	filter("configurations:Debug")
		flags({"Symbols"})
		objdir(PROJECT_FOLDER .. "/intermediate")
		targetdir(PROJECT_FOLDER .. "/debug")

	project("gmsv_filesystem")
		kind("SharedLib")
		defines({
			"FILESYSTEM_SERVER",
			"GMMODULE",
			"GAME_DLL",
			"SUPPRESS_INVALID_PARAMETER_NO_INFO"
		})
		includedirs({
			SOURCE_FOLDER,
			GARRYSMOD_MODULE_BASE_FOLDER .. "/include",
			SCANNING_FOLDER,
			SDK_FOLDER .. "/public",
			SDK_FOLDER .. "/public/tier0",
			SDK_FOLDER .. "/public/tier1"
		})
		files({
			SOURCE_FOLDER .. "/main.cpp",
			SCANNING_FOLDER .. "/symbolfinder.cpp"
		})
		vpaths({
			["Sources"] = {
				SOURCE_FOLDER .. "/**.cpp",
				SCANNING_FOLDER .. "/**.cpp",
				SDK_FOLDER .. "/**.cpp"
			}
		})

		targetprefix("")
		targetextension(".dll")

		filter("system:windows")
			files({SDK_FOLDER .. "/public/tier0/memoverride.cpp"})
			libdirs({SDK_FOLDER .. "/lib/public", GARRYSMOD_MODULE_BASE_FOLDER})
			links({"tier0", "tier1", "lua_shared"})
			targetsuffix("_win32")

			filter({"system:windows", "configurations:Debug"})
				linkoptions({"/NODEFAULTLIB:\"libcmt\""})

		filter("system:linux")
			defines({
				"COMPILER_GCC",
				"POSIX",
				"LINUX",
				"_LINUX",
				"GNUC",
				"NO_MALLOC_OVERRIDE"
			})
			libdirs({SDK_FOLDER .. "/lib/public/linux32"})
			links({"dl", "tier0_srv"})
			prelinkcommands({
				"mkdir -p garrysmod/bin",
				"cp -f ../../" .. GARRYSMOD_MODULE_BASE_FOLDER .. "/lua_shared_srv.so garrysmod/bin"
			})
			linkoptions({
				SDK_FOLDER .. "/lib/public/linux32/tier1.a",
				"-l:garrysmod/bin/lua_shared_srv.so"
			})
			buildoptions({"-std=c++11"})
			targetsuffix("_linux")

		filter("system:macosx")
			libdirs({SDK_FOLDER .. "/lib/public/osx32"})
			links({"dl", "tier0", "tier1", "lua_shared"})
			buildoptions({"-std=c++11"})
			targetsuffix("_mac")

	project("gmcl_filesystem")
		kind("SharedLib")
		defines({
			"FILESYSTEM_CLIENT",
			"GMMODULE",
			"CLIENT_DLL",
			"SUPPRESS_INVALID_PARAMETER_NO_INFO"
		})
		includedirs({
			SOURCE_FOLDER,
			GARRYSMOD_MODULE_BASE_FOLDER .. "/include",
			SCANNING_FOLDER,
			SDK_FOLDER .. "/public",
			SDK_FOLDER .. "/public/tier0",
			SDK_FOLDER .. "/public/tier1"
		})
		files({
			SOURCE_FOLDER .. "/main.cpp",
			SCANNING_FOLDER .. "/symbolfinder.cpp"
		})
		vpaths({
			["Sources"] = {
				SOURCE_FOLDER .. "/**.cpp",
				SCANNING_FOLDER .. "/**.cpp",
				SDK_FOLDER .. "/**.cpp"
			}
		})

		targetprefix("")
		targetextension(".dll")

		filter("system:windows")
			files({SDK_FOLDER .. "/public/tier0/memoverride.cpp"})
			libdirs({SDK_FOLDER .. "/lib/public", GARRYSMOD_MODULE_BASE_FOLDER})
			links({"tier0", "tier1", "lua_shared"})
			targetsuffix("_win32")

			filter({"system:windows", "configurations:Debug"})
				linkoptions({"/NODEFAULTLIB:\"libcmt\""})

		filter("system:linux")
			defines({
				"COMPILER_GCC",
				"POSIX",
				"LINUX",
				"_LINUX",
				"GNUC",
				"NO_MALLOC_OVERRIDE"
			})
			libdirs({SDK_FOLDER .. "/lib/public/linux32"})
			links({"dl", "tier0"})
			prelinkcommands({
				"mkdir -p garrysmod/bin",
				"cp -f ../../" .. GARRYSMOD_MODULE_BASE_FOLDER .. "/lua_shared.so garrysmod/bin"
			})
			linkoptions({
				SDK_FOLDER .. "/lib/public/linux32/tier1.a",
				"-l:garrysmod/bin/lua_shared.so"
			})
			buildoptions({"-std=c++11"})
			targetsuffix("_linux")

		filter("system:macosx")
			libdirs({SDK_FOLDER .. "/lib/public/osx32"})
			links({"dl", "tier0", "tier1", "lua_shared"})
			buildoptions({"-std=c++11"})
			targetsuffix("_mac")