#include <GarrysMod/Lua/Interface.h>
#include <filesystem.hpp>
#include <file.hpp>

#if defined _WIN32 && _MSC_VER != 1600

#error The only supported compilation platform for this project on Windows is Visual Studio 2010 (for ABI reasons).

#endif

GMOD_MODULE_OPEN( )
{
	file::Initialize( state );
	filesystem::Initialize( state );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	filesystem::Deinitialize( state );
	file::Deinitialize( state );
	return 0;
}
