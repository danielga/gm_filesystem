newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://bitbucket.org/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common dir"
})

local gmcommon = _OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON")
if gmcommon == nil then
	error("you didn't provide a path to your garrysmod_common (https://bitbucket.org/danielga/garrysmod_common) directory")
end

include(gmcommon)

CreateSolution("filesystem")
	warnings("Default")

	CreateProject(SERVERSIDE, SOURCES_MANUAL)
		IncludeLuaShared()
		IncludeScanning()
		IncludeSourceSDK()
		AddFiles({"*.cpp", "*.hpp"})

		SetFilter(FILTER_WINDOWS)
			AddFiles({"win32/*.cpp", "win32/*.hpp"})

		SetFilter(FILTER_LINUX, FILTER_MACOSX)
			AddFiles({"posix/*.cpp", "posix/*.hpp"})

	CreateProject(CLIENTSIDE, SOURCES_MANUAL)
		IncludeLuaShared()
		IncludeSourceSDK()
		AddFiles({"*.cpp", "*.hpp"})

		SetFilter(FILTER_WINDOWS)
			AddFiles({"win32/*.cpp", "win32/*.hpp"})

		SetFilter(FILTER_LINUX, FILTER_MACOSX)
			AddFiles({"posix/*.cpp", "posix/*.hpp"})
