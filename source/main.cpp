#include <GarrysMod/Lua/Interface.h>
#include <interfaces.hpp>
#include <lua.hpp>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <interface.h>
#include <filesystem.h>
#include <strtools.h>

#if defined FILESYSTEM_SERVER

#include <symbolfinder.hpp>

#endif

#if defined _WIN32

#include <direct.h>

inline int32_t rmdir( const char *path )
{
	return _rmdir( path );
}

#elif defined __linux || defined __APPLE__

#include <unistd.h>

#endif

namespace global
{

#if defined FILESYSTEM_SERVER

static std::string dedicated_binary = "dedicated.dll";

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

static IFileSystem *filesystem = nullptr;
static const char *vfs_pathid = "VFS";
static const char *vfs_dir = "vfs";
static const char *vfs_path = "garrysmod" CORRECT_PATH_SEPARATOR_S "vfs";

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

	filesystem->CreateDirHierarchy( vfs_dir );
	filesystem->AddSearchPath( vfs_path, "GAME" );
	filesystem->AddSearchPath( vfs_path, vfs_pathid );
}

static void Deinitialize( lua_State *state )
{
	filesystem->RemoveSearchPath( vfs_path, "GAME" );
	filesystem->RemoveSearchPaths( vfs_pathid );
}

}

namespace filehandle
{

static const char *metaname = "FileHandle_t";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT;
static const char *invalid_error = "invalid FileHandle_t";

struct UserData
{
	FileHandle_t file;
	uint8_t type;
	bool invert;
};

inline void CheckType( lua_State *state, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( state, index, metaname );
}

static FileHandle_t Get( lua_State *state, int32_t index, bool *invert = nullptr )
{
	CheckType( state, index );
	UserData *udata = static_cast<UserData *>( LUA->GetUserdata( index ) );
	FileHandle_t file = udata->file;
	if( file == nullptr )
		LUA->ArgError( index, invalid_error );

	if( invert != nullptr )
		*invert = udata->invert;

	return file;
}

static void Create( lua_State *state, FileHandle_t file )
{
	UserData *udata = static_cast<UserData *>( LUA->NewUserdata( sizeof( UserData ) ) );
	udata->file = file;
	udata->type = metatype;
	udata->invert = false;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( state, -2 );
}

template<class Type> inline Type InvertBytes( Type data, bool invert )
{
	if( invert )
	{
		uint8_t *start = reinterpret_cast<uint8_t *>( &data ), *end = start + sizeof( Type ) - 1;
		for( size_t k = 0; k < sizeof( Type ) / 2; ++k )
			*start++ = *end--;
	}

	return data;
}

LUA_FUNCTION_STATIC( tostring )
{
	lua_pushfstring( state, "%s: %p", metaname, Get( state, 1 ) );
	return 1;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( state, 1 ) == Get( state, 2 ) );
	return 1;
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

	global::filesystem->Close( file );
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
	LUA->PushBool( global::filesystem->EndOfFile( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( OK )
{
	LUA->PushBool( global::filesystem->IsOk( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Size )
{
	LUA->PushNumber( global::filesystem->Size( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Tell )
{
	LUA->PushNumber( global::filesystem->Tell( Get( state, 1 ) ) );
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

	global::filesystem->Seek( file, static_cast<int32_t>( pos ), seektype );
	return 0;
}

LUA_FUNCTION_STATIC( Flush )
{
	global::filesystem->Flush( Get( state, 1 ) );
	return 0;
}

LUA_FUNCTION_STATIC( InvertBytes )
{
	CheckType( state, 1 );
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );
	static_cast<UserData *>( LUA->GetUserdata( 1 ) )->invert = LUA->GetBool( 2 );
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

	int32_t read = global::filesystem->Read( buffer.data( ), buffer.size( ), file );
	if( read > 0 )
	{
		LUA->PushString( buffer.data( ), read );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadString )
{
	FileHandle_t file = Get( state, 1 );

	uint32_t pos = global::filesystem->Tell( file );

	char c = '\0';
	std::string buffer;
	while( !global::filesystem->EndOfFile( file ) && global::filesystem->Read( &c, 1, file ) == 1 )
	{
		if( c == '\0' )
		{
			LUA->PushString( buffer.c_str( ) );
			return 1;
		}

		buffer += c;
	}

	global::filesystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
	return 0;
}

LUA_FUNCTION_STATIC( ReadInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( global::filesystem->Tell( file ) + sizeof( int8_t ) >= global::filesystem->Size( file ) )
				return 0;

			int8_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( global::filesystem->Tell( file ) + sizeof( int16_t ) >= global::filesystem->Size( file ) )
				return 0;

			int16_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( global::filesystem->Tell( file ) + sizeof( int32_t ) >= global::filesystem->Size( file ) )
				return 0;

			int32_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( global::filesystem->Tell( file ) + sizeof( int64_t ) >= global::filesystem->Size( file ) )
				return 0;

			int64_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadUInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint8_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint8_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint16_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint16_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint32_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint32_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint64_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint64_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadFloat )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );

	if( global::filesystem->Tell( file ) + sizeof( float ) >= global::filesystem->Size( file ) )
		return 0;

	float num = 0.0f;
	global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( InvertBytes( num, invert ) );
	return 1;
}

LUA_FUNCTION_STATIC( ReadDouble )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );

	if( global::filesystem->Tell( file ) + sizeof( double ) >= global::filesystem->Size( file ) )
		return 0;

	double num = 0.0;
	global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( InvertBytes( num, invert ) );
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

	LUA->PushNumber( len != 0 ? global::filesystem->Write( str, len, file ) : 0.0 );
	return 1;
}

LUA_FUNCTION_STATIC( WriteString )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	if( len > 2147483647 )
		len = 2147483647;

	char z = '\0';
	if( len != 0 )
		LUA->PushNumber(
			global::filesystem->Write( str, len, file ) + global::filesystem->Write( &z, 1, file )
		);
	else
		LUA->PushNumber( 0.0 );

	return 1;
}

LUA_FUNCTION_STATIC( WriteInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			int8_t num = static_cast<int8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			int16_t num = InvertBytes( static_cast<int16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			int32_t num = InvertBytes( static_cast<int32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			int64_t num = InvertBytes( static_cast<int64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteUInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			uint8_t num = static_cast<uint8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			uint16_t num = InvertBytes( static_cast<uint16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			uint32_t num = InvertBytes( static_cast<uint32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			uint64_t num = InvertBytes( static_cast<uint64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteFloat )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	float num = InvertBytes( static_cast<float>( LUA->GetNumber( 2 ) ), invert );
	LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == sizeof( num ) );
	return 1;
}

LUA_FUNCTION_STATIC( WriteDouble )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double num = InvertBytes( LUA->GetNumber( 2 ), invert );
	LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == sizeof( num ) );
	return 1;
}

static void Initialize( lua_State *state )
{
	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

	LUA->PushCFunction( index );
	LUA->SetField( -2, "__index" );

	LUA->PushCFunction( newindex );
	LUA->SetField( -2, "__newindex" );

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

	LUA->PushCFunction( InvertBytes );
	LUA->SetField( -2, "InvertBytes" );

	LUA->PushCFunction( Read );
	LUA->SetField( -2, "Read" );

	LUA->PushCFunction( ReadString );
	LUA->SetField( -2, "ReadString" );

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

	LUA->PushCFunction( WriteString );
	LUA->SetField( -2, "WriteString" );

	LUA->PushCFunction( WriteInt );
	LUA->SetField( -2, "WriteInt" );

	LUA->PushCFunction( WriteUInt );
	LUA->SetField( -2, "WriteUInt" );

	LUA->PushCFunction( WriteFloat );
	LUA->SetField( -2, "WriteFloat" );

	LUA->PushCFunction( WriteDouble );
	LUA->SetField( -2, "WriteDouble" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, metaname );
}

}

namespace filesystem
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

	FileHandle_t f = global::filesystem->Open( filename.c_str( ), options.c_str( ), pathid.c_str( ) );
	if( f == nullptr )
		return 0;

	filehandle::Create( state, f );
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

	LUA->PushBool( global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) );
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

	LUA->PushBool( global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) );
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

	LUA->PushNumber( global::filesystem->GetPathTime( filename.c_str( ), pathid.c_str( ) ) );
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

	if( global::filesystem->IsDirectory( fname.c_str( ), pathid.c_str( ) ) )
	{
		char fullpathold[1024] = { 0 };
		global::filesystem->RelativePathToFullPath_safe( fname.c_str( ), pathid.c_str( ), fullpathold );
		char fullpathnew[1024] = { 0 };
		global::filesystem->RelativePathToFullPath_safe( fnew.c_str( ), pathid.c_str( ), fullpathnew );
		LUA->PushBool( rename( fullpathold, fullpathnew ) == 0 );
	}
	else
	{
		LUA->PushBool( global::filesystem->RenameFile( fname.c_str( ), fnew.c_str( ), pathid.c_str( ) ) );
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

	if( global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) )
	{
		char fullpath[1024] = { 0 };
		global::filesystem->RelativePathToFullPath( filename.c_str( ), pathid.c_str( ), fullpath, sizeof( fullpath ) );
		LUA->PushBool( rmdir( fullpath ) == 0 );
	}
	else if( global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) )
	{
		global::filesystem->RemoveFile( filename.c_str( ), pathid.c_str( ) );
		LUA->PushBool( global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) );
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

	global::filesystem->CreateDirHierarchy( filename.c_str( ), pathid.c_str( ) );
	LUA->PushBool( global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) );
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
	const char *path = global::filesystem->FindFirstEx( filename.c_str( ), pathid.c_str( ), &handle );
	while( path != nullptr )
	{
		bool isdir = global::filesystem->FindIsDirectory( handle );
		LUA->PushNumber( isdir ? ++dirs : ++files );
		LUA->PushString( path );
		LUA->SetTable( isdir ? -3 : -4 );

		path = global::filesystem->FindNext( handle );
	}

	global::filesystem->FindClose( handle );
	return 2;
}

LUA_FUNCTION_STATIC( GetSearchPaths )
{
	const char *pathid = nullptr;
	if( LUA->IsType( 1, GarrysMod::Lua::Type::STRING ) )
		pathid = LUA->GetString( 1 );

	int32_t maxlen = global::filesystem->GetSearchPath( pathid, true, nullptr, 0 );

	LUA->CreateTable( );

	if( maxlen <= 0 )
		return 1;

	std::string paths( maxlen, '\0' );
	global::filesystem->GetSearchPath( pathid, true, &paths[0], maxlen );

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
	LUA->PushBool( global::filesystem->MountSteamContent(
		static_cast<int32_t>( LUA->GetNumber( 1 ) )
	) == FILESYSTEM_MOUNT_OK );
	return 1;
}

LUA_FUNCTION_STATIC( IsLittleEndian )
{
	uint32_t test = 1;
	LUA->PushNumber( *reinterpret_cast<uint8_t *>( &test ) == 1 );
	return 1;
}

static void Initialize( lua_State *state )
{
	LUA->CreateTable( );

	LUA->PushString( "filesystem 1.0.0" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10000 );
	LUA->SetField( -2, "VersionNum" );

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

	LUA->PushCFunction( IsLittleEndian );
	LUA->SetField( -2, "IsLittleEndian" );

	LUA->PushNumber( FILESYSTEM_SEEK_HEAD );
	LUA->SetField( -2, "SEEK_SET" );

	LUA->PushNumber( FILESYSTEM_SEEK_CURRENT );
	LUA->SetField( -2, "SEEK_CUR" );

	LUA->PushNumber( FILESYSTEM_SEEK_TAIL );
	LUA->SetField( -2, "SEEK_END" );

	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

};

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
