#include "file.hpp"
#include "filesystemwrapper.hpp"

#include <filesystem.h>

#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/FactoryLoader.hpp>

#include <cstdint>
#include <string>
#include <tuple>

#if defined FILESYSTEM_SERVER

#include <scanning/symbolfinder.hpp>

#endif

namespace filesystem
{

#if defined FILESYSTEM_SERVER

static SourceSDK::ModuleLoader dedicated_loader( "dedicated" );

#if defined _WIN32

static const char FileSystemFactory_sym[] = "\x55\x8B\xEC\x68\x2A\x2A\x2A\x2A\xFF\x75\x08\xE8";
static const size_t FileSystemFactory_symlen = sizeof( FileSystemFactory_sym ) - 1;

#elif defined __linux

static const char FileSystemFactory_sym[] = "@_Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#elif defined __APPLE__

static const char FileSystemFactory_sym[] = "@__Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#endif

#elif defined FILESYSTEM_CLIENT

static SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio" );

#endif

Wrapper filesystem;

LUA_FUNCTION_STATIC( Open )
{
	file::Base *f = filesystem.Open( LUA->CheckString( 1 ), LUA->CheckString( 2 ), LUA->CheckString( 3 ) );
	if( f == nullptr )
		return 0;

	file::Create( LUA, f );
	return 1;
}

LUA_FUNCTION_STATIC( Exists )
{
	LUA->PushBool( filesystem.Exists( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( IsDirectory )
{
	LUA->PushBool( filesystem.IsDirectory( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetSize )
{
	LUA->PushNumber( static_cast<double>( filesystem.GetSize( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetTime )
{
	LUA->PushNumber( static_cast<double>( filesystem.GetTime( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Rename )
{
	LUA->PushBool( filesystem.Rename( LUA->CheckString( 1 ), LUA->CheckString( 2 ), LUA->CheckString( 3 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	LUA->PushBool( filesystem.Remove( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( MakeDirectory )
{
	LUA->PushBool( filesystem.MakeDirectory( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Find )
{
	std::set<std::string> files, directories;
	std::tie( files, directories ) = filesystem.Find( LUA->CheckString( 1 ), LUA->CheckString( 2 ) );

	LUA->CreateTable( );
	size_t nfiles = 0;
	for( auto it = files.begin( ); it != files.end( ); ++it )
	{
		LUA->PushNumber( static_cast<double>( ++nfiles ) );
		LUA->PushString( it->c_str( ) );
		LUA->SetTable( -3 );
	}

	LUA->CreateTable( );
	size_t ndirs = 0;
	for( auto it = directories.begin( ); it != directories.end( ); ++it )
	{
		LUA->PushNumber( static_cast<double>( ++ndirs ) );
		LUA->PushString( it->c_str( ) );
		LUA->SetTable( -3 );
	}

	return 2;
}

LUA_FUNCTION_STATIC( GetSearchPaths )
{
	if( LUA->GetType( 1 ) <= GarrysMod::Lua::Type::NIL )
	{
		const std::unordered_map<std::string, std::set<std::string>> searchpaths = filesystem.GetSearchPaths( );

		LUA->CreateTable( );
		for( auto it = searchpaths.begin( ); it != searchpaths.end( ); ++it )
		{
			LUA->PushString( it->first.c_str( ) );
			LUA->CreateTable( );

			size_t npaths = 0;
			const std::set<std::string> &paths = it->second;
			for( auto it2 = paths.begin( ); it2 != paths.end( ); ++it2 )
			{
				LUA->PushNumber( static_cast<double>( ++npaths ) );
				LUA->PushString( it2->c_str( ) );
				LUA->SetTable( -3 );
			}

			LUA->SetTable( -3 );
		}
	}
	else
	{
		const std::set<std::string> searchpaths = filesystem.GetSearchPaths( LUA->CheckString( 1 ) );

		LUA->CreateTable( );
		size_t npaths = 0;
		for( auto it = searchpaths.begin( ); it != searchpaths.end( ); ++it )
		{
			LUA->PushNumber( static_cast<double>( ++npaths ) );
			LUA->PushString( it->c_str( ) );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( AddSearchPath )
{
	LUA->PushBool( filesystem.AddSearchPath( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( RemoveSearchPath )
{
	LUA->PushBool( filesystem.RemoveSearchPath( LUA->CheckString( 1 ), LUA->CheckString( 2 ) ) );
	return 1;
}

void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{

#if defined FILESYSTEM_SERVER

	SymbolFinder symfinder;

	CreateInterfaceFn factory = reinterpret_cast<CreateInterfaceFn>( symfinder.Resolve(
		dedicated_loader.GetModule( ), FileSystemFactory_sym, FileSystemFactory_symlen
	) );
	if( factory == nullptr )
		LUA->ThrowError( "unable to retrieve dedicated factory" );

	CBaseFileSystem *fsystem = static_cast<CBaseFileSystem *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );
	if( fsystem == nullptr )
		LUA->ThrowError( "failed to initialize IFileSystem" );

#elif defined FILESYSTEM_CLIENT

	CBaseFileSystem *fsystem = filesystem_loader.GetInterface<CBaseFileSystem>( FILESYSTEM_INTERFACE_VERSION );
	if( fsystem == nullptr )
		LUA->ThrowError( "unable to initialize IFileSystem" );

#endif

	if( !filesystem.Initialize( fsystem ) )
		LUA->ThrowError( "unable to initialize filesystem wrapper" );

	LUA->CreateTable( );

	LUA->PushString( "filesystem 1.4.2" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10402 );
	LUA->SetField( -2, "VersionNum" );

	LUA->PushCFunction( Open );
	LUA->SetField( -2, "Open" );

	LUA->PushCFunction( Exists );
	LUA->SetField( -2, "Exists" );

	LUA->PushCFunction( IsDirectory );
	LUA->SetField( -2, "IsDirectory" );

	LUA->PushCFunction( GetSize );
	LUA->SetField( -2, "GetSize" );

	LUA->PushCFunction( GetTime );
	LUA->SetField( -2, "GetTime" );

	LUA->PushCFunction( Rename );
	LUA->SetField( -2, "Rename" );

	LUA->PushCFunction( Remove );
	LUA->SetField( -2, "Remove" );

	LUA->PushCFunction( MakeDirectory );
	LUA->SetField( -2, "MakeDirectory" );

	LUA->PushCFunction( Find );
	LUA->SetField( -2, "Find" );

	LUA->PushCFunction( GetSearchPaths );
	LUA->SetField( -2, "GetSearchPaths" );

	LUA->PushCFunction( AddSearchPath );
	LUA->SetField( -2, "AddSearchPath" );

	LUA->PushCFunction( RemoveSearchPath );
	LUA->SetField( -2, "RemoveSearchPath" );

	uint32_t test = 1;
	LUA->PushBool( *reinterpret_cast<uint8_t *>( &test ) == 1 );
	LUA->SetField( -2, "IsLittleEndian" );

	LUA->PushNumber( FILESYSTEM_SEEK_HEAD );
	LUA->SetField( -2, "SEEK_SET" );

	LUA->PushNumber( FILESYSTEM_SEEK_CURRENT );
	LUA->SetField( -2, "SEEK_CUR" );

	LUA->PushNumber( FILESYSTEM_SEEK_TAIL );
	LUA->SetField( -2, "SEEK_END" );

	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

};
