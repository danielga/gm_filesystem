#include "filesystem.hpp"
#include "file.hpp"

#include <GarrysMod/Lua/Interface.h>

GMOD_MODULE_OPEN( )
{
	file::Initialize( LUA );
	filesystem::Initialize( LUA );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	filesystem::Deinitialize( LUA );
	file::Deinitialize( LUA );
	return 0;
}
