#include <GarrysMod/Lua/Interface.h>
#include <filesystem.hpp>
#include <file.hpp>

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
