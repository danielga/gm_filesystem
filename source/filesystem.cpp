#include <GarrysMod/Lua/Interface.h>
#include <file.hpp>
#include <main.hpp>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <interface.h>
#include <filesystem.h>
#include <strtools.h>

#if defined _WIN32

#include <direct.h>

inline int32_t rmdir( const char *path )
{
	return _rmdir( path );
}

#elif defined __linux || defined __APPLE__

#include <unistd.h>

#endif

namespace filesystem
{

static const size_t max_searchpath_len = 8192;

static bool IsPathAllowed( std::string &filepath, bool write )
{
	if( !V_RemoveDotSlashes( &filepath[0], CORRECT_PATH_SEPARATOR, true ) )
		return false;

	filepath.resize( std::strlen( filepath.c_str( ) ) );

	for( auto it = filepath.begin( ); it != filepath.end( ); ++it )
	{
		char c = *it;
		if( c < ' ' /*32*/ || c == 127 )
			return false;
	}

	const char *filename = V_GetFileName( filepath.c_str( ) );
	const char *extension = V_GetFileExtension( filename );

// BAD WINDOWS, YOU KNOTTY BOY
#if defined _WIN32

	static const std::unordered_set<std::string> blacklist_filenames = {
		"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8",
		"COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	};

	std::string filename_extless( filename, 0, 4 );
	if( filename_extless[3] == '.' )
		filename_extless.resize( 3 );

	if( blacklist_filenames.find( filename_extless ) != blacklist_filenames.end( ) )
		return false;

#endif

	if( write )
	{
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
		"data", "download", global::vfs_lower_pathid
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

	LUA->CreateTable( );

	std::vector<char> paths( max_searchpath_len, '\0' );
	int32_t len = global::filesystem->GetSearchPath( pathid, true, &paths[0], paths.size( ) );
	if( len <= 0 )
		return 1;

	paths.resize( len );

	size_t k = 0;
	std::vector<char>::iterator start = paths.begin( ), end = paths.end( ), pos = std::find( start, end, ';' );
	for( ; pos != end; start = ++pos, pos = std::find( start, end, ';' ) )
	{
		*pos = '\0';

		LUA->PushNumber( ++k );
		LUA->PushString( &start[0] );
		LUA->SetTable( -3 );
	}

	if( start != end )
	{
		LUA->PushNumber( ++k );
		LUA->PushString( &start[0] );
		LUA->SetTable( -3 );
	}

	return 1;
}

LUA_FUNCTION_STATIC( AddSearchPath )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	std::string directory = LUA->GetString( 1 );
	if( !IsPathAllowed( directory, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	global::filesystem->AddSearchPath( directory.c_str( ), "GAME" );
	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( RemoveSearchPath )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	std::string directory = LUA->GetString( 1 );
	if( !IsPathAllowed( directory, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	LUA->PushBool( global::filesystem->RemoveSearchPath( directory.c_str( ), "GAME" ) );
	return 1;
}

LUA_FUNCTION_STATIC( IsLittleEndian )
{
	uint32_t test = 1;
	LUA->PushNumber( *reinterpret_cast<uint8_t *>( &test ) == 1 );
	return 1;
}

void Initialize( lua_State *state )
{
	LUA->CreateTable( );

	LUA->PushString( "filesystem 1.1.0" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10100 );
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

	LUA->PushCFunction( AddSearchPath );
	LUA->SetField( -2, "AddSearchPath" );

	LUA->PushCFunction( RemoveSearchPath );
	LUA->SetField( -2, "RemoveSearchPath" );

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

void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

};
