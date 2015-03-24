#include <GarrysMod/Lua/Interface.h>
#include <lua.hpp>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <interface.h>
#include <filesystem.h>
#include <strtools.h>
#include <symbolfinder.hpp>

#if defined _WIN32

#include <direct.h>

inline int32_t rmdir( const char *path )
{
	return _rmdir( path );
}

#elif defined __linux || defined __APPLE__

#include <unistd.h>

#endif

namespace Global
{

#if defined FILESYSTEM_SERVER

#if defined _WIN32

static const char *dedicated_lib = "dedicated.dll";

static const char *FileSystemFactory_sym = "\x55\x8B\xEC\x56\x8B\x75\x08\x68\x2A\x2A\x2A\x2A\x56\xE8";
static const size_t FileSystemFactory_symlen = 14;

#elif defined __linux

static const char *dedicated_lib = "dedicated_srv.so";

static const char *FileSystemFactory_sym = "@_Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#elif defined __APPLE__

static const char *dedicated_lib = "dedicated.dylib";

static const char *FileSystemFactory_sym = "@__Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#endif

#elif defined FILESYSTEM_CLIENT

#if defined _WIN32

static CDllDemandLoader factory_loader( "FileSystem_Stdio.dll" );

#elif defined __linux

static CDllDemandLoader factory_loader( "filesystem_stdio.so" );

#elif defined __APPLE__

static CDllDemandLoader factory_loader( "filesystem_stdio.dylib" );

#endif

#endif

static IFileSystem *filesystem = nullptr;
static const char *vfs_path = "vfs";

static void Initialize( lua_State *state )
{

#if defined FILESYSTEM_SERVER

	SymbolFinder symfinder;

	CreateInterfaceFn factory = static_cast<CreateInterfaceFn>(
		symfinder.ResolveOnBinary( dedicated_lib, FileSystemFactory_sym, FileSystemFactory_symlen )
	);
	if( factory == nullptr )
		LUA->ThrowError( "nable to retrieve dedicated factory" );

	filesystem = static_cast<IFileSystem *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );
	if( filesystem == nullptr )
		LUA->ThrowError( "failed to initialize IFileSystem" );

#elif defined FILESYSTEM_CLIENT

	CreateInterfaceFn factory = factory_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "unable to retrieve engine factory" );

	filesystem = static_cast<IFileSystem *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );
	if( filesystem == nullptr )
		LUA->ThrowError( "failed to initialize IFileSystem" );

#endif

	filesystem->AddSearchPath( vfs_path, "GAME" );
	filesystem->AddSearchPath( vfs_path, vfs_path );
}

static void Deinitialize( lua_State *state )
{
	filesystem->RemoveSearchPath( vfs_path, "GAME" );
	filesystem->RemoveSearchPaths( vfs_path );
}

}

namespace FileHandle
{

static const char *metaname = "FileHandle_t";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT;
static const char *invalid_error = "invalid FileHandle_t";

struct UserData
{
	FileHandle_t file;
	uint8_t type;
};

inline void CheckType( lua_State *state, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( state, index, metaname );
}

static FileHandle_t Get( lua_State *state, int32_t index )
{
	CheckType( state, index );
	FileHandle_t file = static_cast<UserData *>( LUA->GetUserdata( index ) )->file;
	if( file == nullptr )
		LUA->ArgError( index, invalid_error );

	return file;
}

static FileHandle_t *Create( lua_State *state )
{
	UserData *udata = static_cast<UserData *>( LUA->NewUserdata( sizeof( UserData ) ) );
	udata->type = metatype;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( state, -2 );

	return &udata->file;
}

LUA_FUNCTION_STATIC( tostring )
{
	lua_pushfstring( state, "%s: %p", metaname, Get( state, 1 ) );
	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( state, 1 ) == Get( state, 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( index )
{
	LUA->GetMetaTable( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	if( !LUA->IsType( -1, GarrysMod::Lua::Type::NIL ) )
		return 1;

	LUA->Pop( 2 );

	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( Close )
{
	CheckType( state, 1 );
	UserData *udata = static_cast<UserData *>( LUA->GetUserdata( 1 ) );

	FileHandle_t file = udata->file;
	if( file == nullptr )
		return 0;

	Global::filesystem->Close( file );
	udata->file = nullptr;
	return 0;
}

LUA_FUNCTION_STATIC( IsValid )
{
	CheckType( state, 1 );
	LUA->PushBool( static_cast<UserData *>( LUA->GetUserdata( 1 ) )->file != nullptr );
	return 1;
}

LUA_FUNCTION_STATIC( EndOfFile )
{
	LUA->PushBool( Global::filesystem->EndOfFile( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( OK )
{
	LUA->PushBool( Global::filesystem->IsOk( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Size )
{
	LUA->PushNumber( Global::filesystem->Size( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Tell )
{
	LUA->PushNumber( Global::filesystem->Tell( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Seek )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double pos = LUA->GetNumber( 2 );
	if( pos < -2147483648.0 || pos > 2147483647.0 )
		LUA->ArgError( 2, "position out of bounds, must fit in a 32 bits signed integer" );

	FileSystemSeek_t seektype = FILESYSTEM_SEEK_HEAD;
	if( LUA->IsType( 3, GarrysMod::Lua::Type::NUMBER ) )
	{
		uint32_t num = static_cast<uint32_t>( LUA->GetNumber( 3 ) );
		if( num >= FILESYSTEM_SEEK_HEAD && num <= FILESYSTEM_SEEK_TAIL )
			seektype = static_cast<FileSystemSeek_t>( num );
	}

	Global::filesystem->Seek( file, static_cast<int32_t>( pos ), seektype );
	return 0;
}

LUA_FUNCTION_STATIC( Flush )
{
	Global::filesystem->Flush( Get( state, 1 ) );
	return 0;
}

LUA_FUNCTION_STATIC( Read )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double len = LUA->GetNumber( 2 );
	if( len < 1.0 || len > 2147483647.0 )
		LUA->ArgError( 2, "size out of bounds, must fit in a 32 bits signed integer and be bigger than 0" );

	std::vector<char> buffer( static_cast<int32_t>( len ) );

	int32_t read = Global::filesystem->Read( buffer.data( ), buffer.size( ), file );
	if( read > 0 )
	{
		LUA->PushString( buffer.data( ), read );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadInt )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( Global::filesystem->Tell( file ) + sizeof( int8_t ) >= Global::filesystem->Size( file ) )
				return 0;

			int8_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( Global::filesystem->Tell( file ) + sizeof( int16_t ) >= Global::filesystem->Size( file ) )
				return 0;

			int16_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 32:
		{
			if( Global::filesystem->Tell( file ) + sizeof( int32_t ) >= Global::filesystem->Size( file ) )
				return 0;

			int32_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 64:
		{
			if( Global::filesystem->Tell( file ) + sizeof( int64_t ) >= Global::filesystem->Size( file ) )
				return 0;

			int64_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadUInt )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( Global::filesystem->Tell( file ) + sizeof( uint8_t ) >= Global::filesystem->Size( file ) )
				return 0;

			uint8_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( Global::filesystem->Tell( file ) + sizeof( uint16_t ) >= Global::filesystem->Size( file ) )
				return 0;

			uint16_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 32:
		{
			if( Global::filesystem->Tell( file ) + sizeof( uint32_t ) >= Global::filesystem->Size( file ) )
				return 0;

			uint32_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 64:
		{
			if( Global::filesystem->Tell( file ) + sizeof( uint64_t ) >= Global::filesystem->Size( file ) )
				return 0;

			uint64_t num = 0;
			Global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadFloat )
{
	FileHandle_t file = Get( state, 1 );

	if( Global::filesystem->Tell( file ) + sizeof( float ) >= Global::filesystem->Size( file ) )
		return 0;

	float num = 0.0f;
	Global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( num );
	return 1;
}

LUA_FUNCTION_STATIC( ReadDouble )
{
	FileHandle_t file = Get( state, 1 );

	if( Global::filesystem->Tell( file ) + sizeof( double ) >= Global::filesystem->Size( file ) )
		return 0;

	double num = 0.0;
	Global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( num );
	return 1;
}

LUA_FUNCTION_STATIC( Write )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	if( len > 2147483647 )
		len = 2147483647;

	LUA->PushNumber( len != 0 ? Global::filesystem->Write( str, len, file ) : 0.0 );
	return 1;
}

LUA_FUNCTION_STATIC( WriteInt )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			int8_t num = static_cast<int8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			int16_t num = static_cast<int16_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			int32_t num = static_cast<int32_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			int64_t num = static_cast<int64_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteUInt )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			uint8_t num = static_cast<uint8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			uint16_t num = static_cast<uint16_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			uint32_t num = static_cast<uint32_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			uint64_t num = static_cast<uint64_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( Global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( WriteFloat )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	float num = static_cast<float>( LUA->GetNumber( 2 ) );
	Global::filesystem->Write( &num, sizeof( num ), file );
	return 0;
}

LUA_FUNCTION_STATIC( WriteDouble )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double num = LUA->GetNumber( 2 );
	Global::filesystem->Write( &num, sizeof( num ), file );
	return 0;
}

static void Initialize( lua_State *state )
{
	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

	LUA->PushCFunction( Close );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( Close );
	LUA->SetField( -2, "Close" );

	LUA->PushCFunction( IsValid );
	LUA->SetField( -2, "IsValid" );

	LUA->PushCFunction( EndOfFile );
	LUA->SetField( -2, "EOF" );

	LUA->PushCFunction( OK );
	LUA->SetField( -2, "OK" );

	LUA->PushCFunction( Size );
	LUA->SetField( -2, "Size" );

	LUA->PushCFunction( Tell );
	LUA->SetField( -2, "Tell" );

	LUA->PushCFunction( Seek );
	LUA->SetField( -2, "Seek" );

	LUA->PushCFunction( Flush );
	LUA->SetField( -2, "Flush" );

	LUA->PushCFunction( Read );
	LUA->SetField( -2, "Read" );

	LUA->PushCFunction( ReadInt );
	LUA->SetField( -2, "ReadInt" );

	LUA->PushCFunction( ReadUInt );
	LUA->SetField( -2, "ReadUInt" );

	LUA->PushCFunction( ReadFloat );
	LUA->SetField( -2, "ReadFloat" );

	LUA->PushCFunction( ReadDouble );
	LUA->SetField( -2, "ReadDouble" );

	LUA->PushCFunction( Write );
	LUA->SetField( -2, "Write" );

	LUA->PushCFunction( WriteInt );
	LUA->SetField( -2, "WriteInt" );

	LUA->PushCFunction( WriteUInt );
	LUA->SetField( -2, "WriteUInt" );

	LUA->PushCFunction( WriteFloat );
	LUA->SetField( -2, "WriteFloat" );

	LUA->PushCFunction( WriteDouble );
	LUA->SetField( -2, "WriteDouble" );

	LUA->Push( -1 );
	LUA->SetField( -2, "__index" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_REG );

	LUA->PushNil( );
	LUA->SetField( -2, metaname );

	LUA->Pop( 1 );
}

}

namespace FileSystem
{

static bool IsPathAllowed( std::string &filename, bool write )
{
	if( !V_RemoveDotSlashes( &filename[0], CORRECT_PATH_SEPARATOR, true ) )
		return false;

	filename.resize( std::strlen( filename.c_str( ) ) );

	if( write )
	{
		const char *extension = V_GetFileExtension( filename.c_str( ) );
		if( extension != nullptr )
		{
			static const std::unordered_set<std::string> whitelist_extensions = {
				// garry's mod
				"lua", "gma", "cache",
				// data
				"txt", "dat",
				// valve
				"nav", "ain", "vpk", "vtf", "vmt", "mdl", "vtx", "phy", "vvd", "pcf", "bsp",
				// image
				"tga", "jpg", "png",
				// audio
				"wav", "mp3",
				// video
				"mp4", "ogg", "avi", "mkv",
				// fonts
				"ttf", "ttc",
				// assorted
				"tmp", "md", "db", "inf"
			};

			std::string ext = extension;
			std::transform( ext.begin( ), ext.end( ), ext.begin( ), tolower );
			return whitelist_extensions.find( extension ) != whitelist_extensions.end( );
		}
	}

	return true;
}

static bool IsPathIDAllowed( std::string &pathid, bool write )
{
	static const std::unordered_set<std::string> whitelist_pathid_read = {
		"data", "download", "lua", "lcl", "lsv", "game", "gamebin", "mod"
	};

	static const std::unordered_set<std::string> whitelist_pathid_write = {
		"data", "download"
	};

	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), tolower );
	const auto &whitelist = write ? whitelist_pathid_write : whitelist_pathid_read;
	return whitelist.find( pathid ) != whitelist.end( );
}

LUA_FUNCTION_STATIC( Open )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), options = LUA->GetString( 2 ), pathid = LUA->GetString( 3 );

	std::transform( options.begin( ), options.end( ), options.begin( ), tolower );
	bool write = options.find_first_of( "wa+" ) != options.npos;

	if( !IsPathAllowed( filename, write ) || !IsPathIDAllowed( pathid, write ) )
		return 0;

	FileHandle_t f = Global::filesystem->Open( filename.c_str( ), options.c_str( ), pathid.c_str( ) );
	if( f == nullptr )
		return 0;

	*FileHandle::Create( state ) = f;
	return 1;
}

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, false ) || !IsPathIDAllowed( pathid, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	LUA->PushBool( Global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( IsDirectory )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, false ) || !IsPathIDAllowed( pathid, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	LUA->PushBool( Global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( GetTime )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, false ) || !IsPathIDAllowed( pathid, false ) )
	{
		LUA->PushNumber( -1 );
		return 1;
	}

	LUA->PushNumber( Global::filesystem->GetPathTime( filename.c_str( ), pathid.c_str( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Rename )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	std::string fname = LUA->GetString( 1 ), fnew = LUA->GetString( 2 ), pathid = LUA->GetString( 3 );

	if( !IsPathAllowed( fname, true ) || !IsPathAllowed( fnew, true ) || !IsPathIDAllowed( pathid, true ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( Global::filesystem->IsDirectory( fname.c_str( ), pathid.c_str( ) ) )
	{
		char fullpathold[1024] = { 0 };
		Global::filesystem->RelativePathToFullPath_safe( fname.c_str( ), pathid.c_str( ), fullpathold );
		char fullpathnew[1024] = { 0 };
		Global::filesystem->RelativePathToFullPath_safe( fnew.c_str( ), pathid.c_str( ), fullpathnew );
		LUA->PushBool( rename( fullpathold, fullpathnew ) == 0 );
	}
	else
	{
		LUA->PushBool( Global::filesystem->RenameFile( fname.c_str( ), fnew.c_str( ), pathid.c_str( ) ) );
	}

	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, true ) || !IsPathIDAllowed( pathid, true ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( Global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) )
	{
		char fullpath[1024] = { 0 };
		Global::filesystem->RelativePathToFullPath( filename.c_str( ), pathid.c_str( ), fullpath, sizeof( fullpath ) );
		LUA->PushBool( rmdir( fullpath ) == 0 );
	}
	else if( Global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) )
	{
		Global::filesystem->RemoveFile( filename.c_str( ), pathid.c_str( ) );
		LUA->PushBool( Global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) );
	}

	return 1;
}

LUA_FUNCTION_STATIC( MakeDirectory )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, true ) || !IsPathIDAllowed( pathid, true ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	Global::filesystem->CreateDirHierarchy( filename.c_str( ), pathid.c_str( ) );
	LUA->PushBool( Global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Find )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );

	LUA->CreateTable( );
	LUA->CreateTable( );

	if( !IsPathAllowed( filename, false ) || !IsPathIDAllowed( pathid, false ) )
		return 2;

	size_t files = 0, dirs = 0;
	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *path = Global::filesystem->FindFirstEx( filename.c_str( ), pathid.c_str( ), &handle );
	while( path != nullptr )
	{
		bool isdir = Global::filesystem->FindIsDirectory( handle );
		LUA->PushNumber( isdir ? ++dirs : ++files );
		LUA->PushString( path );
		LUA->SetTable( isdir ? -3 : -4 );

		path = Global::filesystem->FindNext( handle );
	}

	Global::filesystem->FindClose( handle );
	return 2;
}

LUA_FUNCTION_STATIC( GetSearchPaths )
{
	const char *pathid = nullptr;
	if( LUA->IsType( 1, GarrysMod::Lua::Type::STRING ) )
		pathid = LUA->GetString( 1 );

	int32_t maxlen = Global::filesystem->GetSearchPath( pathid, true, nullptr, 0 );

	LUA->CreateTable( );

	if( maxlen <= 0 )
		return 1;

	std::string paths( maxlen, '\0' );
	Global::filesystem->GetSearchPath( pathid, true, &paths[0], maxlen );

	size_t k = 0, start = 0, pos = paths.find( ';' );
	for( ; pos != paths.npos; start = pos + 1, pos = paths.find( ';', start ) )
	{
		paths[pos] = '\0';

		LUA->PushNumber( ++k );
		LUA->PushString( &paths[start] );
		LUA->SetTable( -3 );
	}

	LUA->PushNumber( ++k );
	LUA->PushString( &paths[start] );
	LUA->SetTable( -3 );
	return 1;
}

LUA_FUNCTION_STATIC( MountSteamContent )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::NUMBER );
	LUA->PushBool( Global::filesystem->MountSteamContent(
		static_cast<int32_t>( LUA->GetNumber( 1 ) )
	) == FILESYSTEM_MOUNT_OK );
	return 1;
}

static void Initialize( lua_State *state )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->CreateTable( );

	LUA->PushCFunction( Open );
	LUA->SetField( -2, "Open" );

	LUA->PushCFunction( Exists );
	LUA->SetField( -2, "Exists" );

	LUA->PushCFunction( IsDirectory );
	LUA->SetField( -2, "IsDirectory" );

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

	LUA->PushCFunction( MountSteamContent );
	LUA->SetField( -2, "MountSteamContent" );

	LUA->PushNumber( FILESYSTEM_SEEK_HEAD );
	LUA->SetField( -2, "SEEK_SET" );

	LUA->PushNumber( FILESYSTEM_SEEK_CURRENT );
	LUA->SetField( -2, "SEEK_CURRENT" );

	LUA->PushNumber( FILESYSTEM_SEEK_TAIL );
	LUA->SetField( -2, "SEEK_END" );

	LUA->SetField( -2, "filesystem" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->PushNil( );
	LUA->SetField( -2, "filesystem" );

	LUA->Pop( 1 );
}

};

GMOD_MODULE_OPEN( )
{
	Global::Initialize( state );
	FileHandle::Initialize( state );
	FileSystem::Initialize( state );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	FileSystem::Deinitialize( state );
	FileHandle::Deinitialize( state );
	Global::Deinitialize( state );
	return 0;
}