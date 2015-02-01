#include <GarrysMod/Lua/Interface.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <interface.h>
#include <filesystem.h>
#include <strtools.h>

#if defined _WIN32

#include <direct.h>

#define rmdir _rmdir

#elif defined __linux || defined __APPLE__

#include <unistd.h>

#endif

namespace filesystem
{

#if defined _WIN32

static CDllDemandLoader internal_loader( "FileSystem_Stdio.dll" );

#elif defined __linux

static CDllDemandLoader internal_loader( "filesystem_stdio.so" );

#elif defined __APPLE__

static CDllDemandLoader internal_loader( "filesystem_stdio.dylib" );

#endif

static IFileSystem *internal = nullptr;

static const char *vfs_path = "vfs";

static void Initialize( lua_State *state )
{
	CreateInterfaceFn factory = internal_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "Couldn't get filesystem_stdio factory. Critical error." );

	internal = static_cast<IFileSystem *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );
	if( internal == nullptr )
		LUA->ThrowError( "IFileSystem not initialized. Critical error." );

	internal->AddSearchPath( vfs_path, "GAME" );
	internal->AddSearchPath( vfs_path, vfs_path );
}

static void Deinitialize( lua_State *state )
{
	internal->RemoveSearchPaths( vfs_path );
}

}

namespace file
{

static const char *metaname = "file";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT;
static const char *invalid_error = "file object is not valid";

typedef GarrysMod::Lua::UserData userdata;

inline userdata *GetUserdata( lua_State *state, int index )
{
	return static_cast<userdata *>( LUA->GetUserdata( index ) );
}

inline FileHandle_t GetAndValidateFile( lua_State *state, int index, const char *err )
{
	FileHandle_t file = static_cast<FileHandle_t>( GetUserdata( state, index )->data );
	if( file == nullptr )
		LUA->ThrowError( err );

	return file;
}

static userdata *Create( lua_State *state )
{
	userdata *udata = static_cast<userdata *>( LUA->NewUserdata( sizeof( userdata ) ) );
	udata->type = metatype;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	return udata;
}

LUA_FUNCTION_STATIC( tostring )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	char formatted[30] = { 0 };
	V_snprintf( formatted, sizeof( formatted ), "%s: %p", metaname, file );
	LUA->PushString( formatted );

	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, metatype );

	FileHandle_t file1 = GetAndValidateFile( state, 1, invalid_error );
	FileHandle_t file2 = GetAndValidateFile( state, 2, invalid_error );

	LUA->PushBool( file1 == file2 );

	return 0;
}

LUA_FUNCTION_STATIC( Close )
{
	LUA->CheckType( 1, metatype );

	userdata *udata = GetUserdata( state, 1 );

	FileHandle_t file = static_cast<FileHandle_t>( udata->data );
	if( file == nullptr )
		return 0;

	filesystem::internal->Close( file );

	udata->data = nullptr;

	return 0;
}

LUA_FUNCTION_STATIC( IsValid )
{
	LUA->CheckType( 1, metatype );

	userdata *udata = GetUserdata( state, 1 );

	LUA->PushBool( udata->data != nullptr );

	return 1;
}

LUA_FUNCTION_STATIC( EndOfFile )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	LUA->PushBool( filesystem::internal->EndOfFile( file ) );

	return 1;
}

LUA_FUNCTION_STATIC( OK )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	LUA->PushBool( filesystem::internal->IsOk( file ) );

	return 1;
}

LUA_FUNCTION_STATIC( Size )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	LUA->PushNumber( filesystem::internal->Size( file ) );

	return 1;
}

LUA_FUNCTION_STATIC( Tell )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	LUA->PushNumber( filesystem::internal->Tell( file ) );

	return 1;
}

LUA_FUNCTION_STATIC( Seek )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	double num = LUA->GetNumber( 2 );
	if( num < -2147483648.0 || num > 2147483647.0 )
		LUA->ThrowError( "size out of bounds (must fit in a 32 bits signed integer)" );

	FileSystemSeek_t seektype = FILESYSTEM_SEEK_HEAD;
	if( LUA->IsType( 3, GarrysMod::Lua::Type::NUMBER ) )
	{
		uint32_t num = static_cast<uint32_t>( LUA->GetNumber( 3 ) );
		if( num == FILESYSTEM_SEEK_HEAD || num == FILESYSTEM_SEEK_CURRENT || num == FILESYSTEM_SEEK_TAIL )
			seektype = static_cast<FileSystemSeek_t>( num );
	}

	filesystem::internal->Seek( file, static_cast<int>( num ), seektype );

	return 0;
}

LUA_FUNCTION_STATIC( Flush )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	filesystem::internal->Flush( file );

	return 0;
}

LUA_FUNCTION_STATIC( Read )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	double num = LUA->GetNumber( 2 );
	if( num < 1.0 || num > 2147483647.0 )
		LUA->ThrowError( "size out of bounds (must fit in a 32 bits signed integer and be bigger than 0)" );

	int len = static_cast<int>( num );
	std::vector<char> buffer( len );

	int read = filesystem::internal->Read( buffer.data( ), len, file );
	if( read > 0 )
	{
		LUA->PushString( buffer.data( ), read );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadInt )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	int64_t bits = LUA->GetNumber( 2 );
	switch( bits )
	{
		case 8:
		{
			if( filesystem::internal->Tell( file ) + sizeof( int8_t ) >= filesystem::internal->Size( file ) )
				return 0;

			int8_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( filesystem::internal->Tell( file ) + sizeof( int16_t ) >= filesystem::internal->Size( file ) )
				return 0;

			int16_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 32:
		{
			if( filesystem::internal->Tell( file ) + sizeof( int32_t ) >= filesystem::internal->Size( file ) )
				return 0;

			int32_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 64:
		{
			if( filesystem::internal->Tell( file ) + sizeof( int64_t ) >= filesystem::internal->Size( file ) )
				return 0;

			int64_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		default:
			LUA->ThrowError( "number of bits requested is not supported (must be 8, 16, 32 or 64)" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadUInt )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	int64_t bits = LUA->GetNumber( 2 );
	switch( bits )
	{
		case 8:
		{
			if( filesystem::internal->Tell( file ) + sizeof( uint8_t ) >= filesystem::internal->Size( file ) )
				return 0;

			uint8_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( filesystem::internal->Tell( file ) + sizeof( uint16_t ) >= filesystem::internal->Size( file ) )
				return 0;

			uint16_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 32:
		{
			if( filesystem::internal->Tell( file ) + sizeof( uint32_t ) >= filesystem::internal->Size( file ) )
				return 0;

			uint32_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 64:
		{
			if( filesystem::internal->Tell( file ) + sizeof( uint64_t ) >= filesystem::internal->Size( file ) )
				return 0;

			uint64_t num = 0;
			filesystem::internal->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		default:
			LUA->ThrowError( "number of bits requested is not supported (must be 8, 16, 32 or 64)" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadFloat )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	if( filesystem::internal->Tell( file ) + sizeof( float ) >= filesystem::internal->Size( file ) )
		return 0;

	float num = 0.0f;
	filesystem::internal->Read( &num, sizeof( num ), file );
	LUA->PushNumber( num );

	return 1;
}

LUA_FUNCTION_STATIC( ReadDouble )
{
	LUA->CheckType( 1, metatype );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	if( filesystem::internal->Tell( file ) + sizeof( double ) >= filesystem::internal->Size( file ) )
		return 0;

	double num = 0.0;
	filesystem::internal->Read( &num, sizeof( num ), file );
	LUA->PushNumber( num );

	return 1;
}

LUA_FUNCTION_STATIC( Write )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	if( len > 2147483647 )
		len = 2147483647;

	LUA->PushNumber( len != 0 ? filesystem::internal->Write( str, len, file ) : 0.0 );
	return 1;
}

LUA_FUNCTION_STATIC( WriteInt )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	int64_t bits = LUA->GetNumber( 3 );
	switch( bits )
	{
		case 8:
		{
			int8_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			int16_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			int32_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			int64_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ThrowError( "number of bits requested is not supported (must be 8, 16, 32 or 64)" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteUInt )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	int64_t bits = LUA->GetNumber( 3 );
	switch( bits )
	{
		case 8:
		{
			uint8_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			uint16_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			uint32_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			uint64_t num = LUA->GetNumber( 2 );
			LUA->PushBool( filesystem::internal->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		default:
			LUA->ThrowError( "number of bits requested is not supported (must be 8, 16, 32 or 64)" );
	}

	return 0;
}

LUA_FUNCTION_STATIC( WriteFloat )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	float num = LUA->GetNumber( 2 );
	filesystem::internal->Write( &num, sizeof( num ), file );

	return 0;
}

LUA_FUNCTION_STATIC( WriteDouble )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	FileHandle_t file = GetAndValidateFile( state, 1, invalid_error );

	double num = LUA->GetNumber( 2 );
	filesystem::internal->Write( &num, sizeof( num ), file );

	return 0;
}

static void RegisterMetaTable( lua_State *state )
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
}

}

namespace filesystem
{

static const std::unordered_set<std::string> whitelist_pathid_read = {
	"data",
	"download",
	"lua",
	"lcl",
	"lsv",
	"game",
	"gamebin",
	"mod"
};

static const std::unordered_set<std::string> whitelist_pathid_write = {
	"data",
	"download"
};

static const std::unordered_set<std::string> whitelist_extensions = {
	// garry's mod
	"lua",
	"gma",
	"cache",
	
	// data
	"txt",
	"dat",

	// valve
	"nav",
	"ain",
	"vpk",
	"vtf",
	"vmt",
	"mdl",
	"vtx",
	"phy",
	"vvd",
	"pcf",
	"bsp",

	// image
	"tga",
	"jpg",
	"png",

	// audio
	"wav",
	"mp3",

	// video
	"mp4",
	"ogg",
	"avi",
	"mkv",

	// fonts
	"ttf",
	"ttc",

	// assorted
	"tmp",
	"md",
	"db",
	"inf"
};

static bool IsPathAllowed( std::string filename, std::string options, std::string pathid )
{
	std::transform( options.begin( ), options.end( ), options.begin( ), tolower );
	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), tolower );

	bool wantswrite = options.find_first_of( "wa+" ) != options.npos;

	const std::unordered_set<std::string> &whitelist = wantswrite ? whitelist_pathid_write : whitelist_pathid_read;
	if( whitelist.find( pathid ) == whitelist.end( ) )
		return false;

	if( !V_RemoveDotSlashes( &filename[0], CORRECT_PATH_SEPARATOR, true ) )
		return false;

	if( wantswrite )
	{
		const char *extension = V_GetFileExtension( filename.c_str( ) );
		if( extension != nullptr && whitelist_extensions.find( extension ) == whitelist_extensions.end( ) )
			return false;
	}

	return true;
}

LUA_FUNCTION_STATIC( Open )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *options = LUA->GetString( 2 ), *pathid = LUA->GetString( 3 );

	if( !IsPathAllowed( filename, options, pathid ) )
		return 0;

	FileHandle_t f = internal->Open( filename, options, pathid );
	if( f == nullptr )
		return 0;

	file::Create( state )->data = f;

	return 1;
}

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, "r", pathid ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	LUA->PushBool( internal->FileExists( filename, pathid ) );

	return 1;
}

LUA_FUNCTION_STATIC( IsDirectory )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, "r", pathid ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	LUA->PushBool( internal->IsDirectory( filename, pathid ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetTime )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, "r", pathid ) )
	{
		LUA->PushNumber( -1 );
		return 1;
	}

	LUA->PushNumber( internal->GetPathTime( filename, pathid ) );

	return 1;
}

LUA_FUNCTION_STATIC( Rename )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	const char *filenameold = LUA->GetString( 1 ), *filenamenew = LUA->GetString( 2 ), *pathid = LUA->GetString( 3 );

	if( !IsPathAllowed( filenameold, "w", pathid ) && !IsPathAllowed( filenamenew, "w", pathid ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( internal->IsDirectory( filenameold, pathid ) )
	{
		char fullpathold[1024] = { 0 };
		internal->RelativePathToFullPath_safe( filenameold, pathid, fullpathold, FILTER_CULLPACK );
		char fullpathnew[1024] = { 0 };
		internal->RelativePathToFullPath_safe( filenamenew, pathid, fullpathnew, FILTER_CULLPACK );
		LUA->PushBool( rename( fullpathold, fullpathnew ) == 0 );
	}
	else
	{
		LUA->PushBool( internal->RenameFile( filenameold, filenamenew, pathid ) );
	}

	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, "w", pathid ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( internal->IsDirectory( filename, pathid ) )
	{
		char fullpath[1024] = { 0 };
		internal->RelativePathToFullPath( filename, pathid, fullpath, sizeof( fullpath ), FILTER_CULLPACK );
		LUA->PushBool( rmdir( fullpath ) == 0 );
	}
	else if( internal->FileExists( filename, pathid ) )
	{
		internal->RemoveFile( filename, pathid );
		LUA->PushBool( internal->FileExists( filename, pathid ) );
	}

	return 1;
}

LUA_FUNCTION_STATIC( MakeDirectory )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	if( !IsPathAllowed( filename, "w", pathid ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	internal->CreateDirHierarchy( filename, pathid );
	LUA->PushBool( internal->IsDirectory( filename, pathid ) );

	return 1;
}

LUA_FUNCTION_STATIC( Find )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	const char *filename = LUA->GetString( 1 ), *pathid = LUA->GetString( 2 );

	LUA->CreateTable( );
	LUA->CreateTable( );

	if( !IsPathAllowed( filename, "r", pathid ) )
		return 2;

	size_t k = 0;
	FileFindHandle_t handle = -1;
	const char *path = internal->FindFirstEx( filename, pathid, &handle );
	while( path != nullptr )
	{
		LUA->PushNumber( ++k );
		LUA->PushString( path );
		LUA->SetTable( internal->FindIsDirectory( handle ) ? -4 : -3 );

		path = internal->FindNext( handle );
	}

	internal->FindClose( handle );

	return 2;
}

LUA_FUNCTION_STATIC( GetSearchPaths )
{
	const char *pathid = nullptr;
	if( LUA->IsType( 1, GarrysMod::Lua::Type::STRING ) )
		pathid = LUA->GetString( 1 );

	int maxlen = internal->GetSearchPath( pathid, true, nullptr, 0 );

	LUA->CreateTable( );

	if( maxlen <= 0 )
		return 1;

	std::string paths( maxlen, '\0' );
	internal->GetSearchPath( pathid, true, &paths[0], maxlen );

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

	LUA->PushBool( internal->MountSteamContent( static_cast<int>( LUA->GetNumber( 1 ) ) ) == FILESYSTEM_MOUNT_OK );

	return 1;
}

static void RegisterGlobalTable( lua_State *state )
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
}

}

GMOD_MODULE_OPEN( )
{
	filesystem::Initialize( state );

	file::RegisterMetaTable( state );

	filesystem::RegisterGlobalTable( state );

	return 0;
}

GMOD_MODULE_CLOSE( )
{
	filesystem::Deinitialize( state );

	return 0;
}