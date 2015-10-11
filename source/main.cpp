#include <GarrysMod/Lua/Interface.h>
#include <filesystem.hpp>
#include <file.hpp>
#include <interfaces.hpp>
#include <string>
#include <filesystem.h>

#if defined FILESYSTEM_SERVER

#include <symbolfinder.hpp>

#endif

namespace global
{

#if defined FILESYSTEM_SERVER

static std::string dedicated_binary = helpers::GetBinaryFileName( "dedicated", false, true, "bin/" );

#if defined _WIN32

static const char *FileSystemFactory_sym = "\x55\x8B\xEC\x56\x8B\x75\x08\x68\x2A\x2A\x2A\x2A\x56\xE8";
static const size_t FileSystemFactory_symlen = 14;

#elif defined __linux

static const char *FileSystemFactory_sym = "@_Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#elif defined __APPLE__

static const char *FileSystemFactory_sym = "@__Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#endif

#elif defined FILESYSTEM_CLIENT

static SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio", false, false );

#endif

IFileSystem *filesystem = nullptr;

static void Initialize( lua_State *state )
{

#if defined FILESYSTEM_SERVER

	SymbolFinder symfinder;

	CreateInterfaceFn factory = static_cast<CreateInterfaceFn>( symfinder.ResolveOnBinary(
		dedicated_binary.c_str( ), FileSystemFactory_sym, FileSystemFactory_symlen
	) );
	if( factory == nullptr )
		LUA->ThrowError( "unable to retrieve dedicated factory" );

	filesystem = static_cast<IFileSystem *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );
	if( filesystem == nullptr )
		LUA->ThrowError( "failed to initialize IFileSystem" );

#elif defined FILESYSTEM_CLIENT

	filesystem = filesystem_loader.GetInterface<IFileSystem>( FILESYSTEM_INTERFACE_VERSION );
	if( filesystem == nullptr )
		LUA->ThrowError( "unable to initialize IFileSystem" );

#endif
}

static void Deinitialize( lua_State *state )
{ }

}

GMOD_MODULE_OPEN( )
{
	global::Initialize( state );
	filehandle::Initialize( state );
	filesystem::Initialize( state );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	filesystem::Deinitialize( state );
	filehandle::Deinitialize( state );
	global::Deinitialize( state );
	return 0;
}
