newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(path.join(gmcommon, "generator.v2.lua"))

CreateWorkspace({name = "filesystem", abi_compatible = true})
	CreateProject({serverside = true})
		IncludeLuaShared()
		IncludeScanning()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()

		filter("system:windows")
			files({"source/win32/*.cpp", "source/win32/*.hpp"})

		filter("system:linux or macosx")
			files({"source/posix/*.cpp", "source/posix/*.hpp"})

	CreateProject({serverside = false})
		IncludeLuaShared()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()

		filter("system:windows")
			files({"source/win32/*.cpp", "source/win32/*.hpp"})

		filter("system:linux or macosx")
			files({"source/posix/*.cpp", "source/posix/*.hpp"})
