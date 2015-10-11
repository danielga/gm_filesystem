#include <GarrysMod/Lua/Interface.h>
#include <file.hpp>
#include <main.hpp>
#include <cstdint>
#include <cctype>
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

enum class WhitelistType
{
	Read,
	Write,
	SearchPath
};

static const size_t max_searchpath_len = 8192;
static const size_t max_tempbuffer_len = 16384;
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
static const std::unordered_set<std::string> whitelist_pathid[] = {
	{
		"data", "download", "lua", "lcl", "lsv", "game", "garrysmod", "gamebin", "mod",
		"base_path", "executable_path", "default_write_path"
	}, // Read
	{ "data", "download" }, // Write
	{ "game", "lcl" } // SearchPath
};

static std::string garrysmod_fullpath;

static bool IsPathIDAllowed( std::string &pathid, WhitelistType whitelist_type )
{
	if( pathid.empty( ) )
		return false;

	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), std::tolower );
	const auto &whitelist = whitelist_pathid[static_cast<size_t>( whitelist_type )];
	return whitelist.find( pathid ) != whitelist.end( );
}

inline bool FixupFilePath( std::string &filepath, const std::string &pathid )
{
	if( filepath.empty( ) )
		return false;

	if( V_IsAbsolutePath( filepath.c_str( ) ) )
	{
		std::string tpath = filepath;
		filepath.resize( max_tempbuffer_len );
		if( !global::filesystem->FullPathToRelativePathEx( tpath.c_str( ), pathid.c_str( ), &filepath[0], filepath.size( ) ) )
			return false;

		if( filepath.compare( 0, 3, ".." CORRECT_PATH_SEPARATOR_S ) == 0 )
			return false;
	}
	else if( !V_RemoveDotSlashes( &filepath[0] ) )
	{
		return false;
	}

	filepath.resize( std::strlen( filepath.c_str( ) ) );
	return true;
}

#if defined _WIN32

template<typename Input>
inline Input UTF8Decode( Input begin, Input end, uint32_t &output, uint32_t replace = 0 )
{
	static const uint32_t offsets[] = {
		0x00000000,
		0x00003080,
		0x000E2080,
		0x03C82080
	};

	uint8_t ch = static_cast<uint8_t>( *begin );
	int32_t trailingBytes = -1;
	if( ch < 128 )
		trailingBytes = 0;
	else if( ch < 192 )
		/* do nothing, invalid byte */;
	else if( ch < 224 )
		trailingBytes = 1;
	else if( ch < 240 )
		trailingBytes = 2;
	else if( ch < 248 )
		trailingBytes = 3;
	else
		/* do nothing, invalid byte, used for 5 and 6 bytes UTF8 sequences */;

	if( trailingBytes == -1 )
	{
		++begin;
		output = replace;
	}
	else if( begin + trailingBytes < end )
	{
		output = 0;
		switch( trailingBytes )
		{
		case 3:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 2:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 1:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 0:
			output += static_cast<uint8_t>( *begin );
			++begin;
		}

		output -= offsets[trailingBytes];
	}
	else
	{
		begin = end;
		output = replace;
	}

	return begin;
}

#endif

inline bool VerifyFilePath( const std::string &filepath, bool find )
{
// BAD WINDOWS, YOU KNOTTY BOY
#if defined _WIN32

	static const std::unordered_set<uint32_t> blacklist_characters = {
		'<', '>', ':', '"', '/', '|', '?'
		// '\\' is another one but is used as path separator
		// '*' is also blacklisted but can be used for finding
	};

	auto begin = filepath.begin( ), end = filepath.end( );
	uint32_t out = 0;
	do
	{
		begin = UTF8Decode( begin, end, out );
		if( out < 32 || ( !find && out == '*' || blacklist_characters.find( out ) != blacklist_characters.end( ) ) )
			return false;
	}
	while( begin != end );

	const char *filename = V_GetFileName( filepath.c_str( ) );
	std::string filename_extless( filename, 0, 4 );
	if( filename_extless.size( ) >= 3 )
	{
		static const std::unordered_set<std::string> blacklist_filenames = {
			"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
			"COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
		};

		if( filename_extless.size( ) >= 4 && filename_extless[3] == '.' )
			filename_extless.resize( 3 );

		if( blacklist_filenames.find( filename_extless ) != blacklist_filenames.end( ) )
			return false;
	}

#endif

	return true;
}

inline bool VerifyExtension( const std::string &filepath, WhitelistType whitelist_type )
{
	const char *extension = V_GetFileExtension( filepath.c_str( ) );
	if( whitelist_type == WhitelistType::Write && extension != nullptr )
	{
		std::string ext = extension;
		std::transform( ext.begin( ), ext.end( ), ext.begin( ), std::tolower );
		if( whitelist_extensions.find( extension ) == whitelist_extensions.end( ) )
			return false;
	}

	return true;
}

static bool IsPathAllowed(
	std::string &filepath,
	std::string &pathid,
	WhitelistType whitelist_type,
	bool find = false
)
{
	return FixupFilePath( filepath, pathid ) &&
		VerifyFilePath( filepath, find ) &&
		VerifyExtension( filepath, whitelist_type );
}

LUA_FUNCTION_STATIC( Open )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 3, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), options = LUA->GetString( 2 ), pathid = LUA->GetString( 3 );

	std::transform( options.begin( ), options.end( ), options.begin( ), std::tolower );
	WhitelistType wtype = options.find_first_of( "wa+" ) != options.npos ?
		WhitelistType::Write : WhitelistType::Read;

	if( !IsPathIDAllowed( pathid, wtype ) || !IsPathAllowed( filename, pathid, wtype ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( fname, pathid, WhitelistType::Write ) ||
		!IsPathAllowed( fnew, pathid, WhitelistType::Write ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( global::filesystem->IsDirectory( fname.c_str( ), pathid.c_str( ) ) )
	{
		char fullpathold[max_tempbuffer_len] = { 0 };
		global::filesystem->RelativePathToFullPath_safe( fname.c_str( ), pathid.c_str( ), fullpathold );
		char fullpathnew[max_tempbuffer_len] = { 0 };
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Write ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	if( global::filesystem->IsDirectory( filename.c_str( ), pathid.c_str( ) ) )
	{
		char fullpath[max_tempbuffer_len] = { 0 };
		global::filesystem->RelativePathToFullPath( filename.c_str( ), pathid.c_str( ), fullpath, sizeof( fullpath ) );
		LUA->PushBool( rmdir( fullpath ) == 0 );
	}
	else if( global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) )
	{
		global::filesystem->RemoveFile( filename.c_str( ), pathid.c_str( ) );
		LUA->PushBool( !global::filesystem->FileExists( filename.c_str( ), pathid.c_str( ) ) );
	}

	return 1;
}

LUA_FUNCTION_STATIC( MakeDirectory )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string filename = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Write ) )
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

	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read, true ) )
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
	int32_t len = global::filesystem->GetSearchPath( pathid, true, &paths[0], paths.size( ) ) - 1;
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
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	std::string directory = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );
	if( !IsPathIDAllowed( pathid, WhitelistType::SearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	directory.insert( 0, garrysmod_fullpath );
	global::filesystem->AddSearchPath( directory.c_str( ), pathid.c_str( ) );
	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( RemoveSearchPath )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	std::string directory = LUA->GetString( 1 ), pathid = LUA->GetString( 2 );
	if( !IsPathIDAllowed( pathid, WhitelistType::SearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false ) )
	{
		LUA->PushBool( false );
		return 1;
	}

	directory.insert( 0, garrysmod_fullpath );
	LUA->PushBool( global::filesystem->RemoveSearchPath( directory.c_str( ), pathid.c_str( ) ) );
	return 1;
}

void Initialize( lua_State *state )
{
	std::string fullpath;
	fullpath.resize( max_tempbuffer_len );
	int32_t len = global::filesystem->GetSearchPath( "DEFAULT_WRITE_PATH", false, &fullpath[0], fullpath.size( ) ) - 1;
	if( len <= 0 )
		LUA->ThrowError( "unable to get path to Garry's Mod directory" );

	fullpath.resize( len );

	garrysmod_fullpath = fullpath;

	LUA->CreateTable( );

	LUA->PushString( "filesystem 1.2.0" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10200 );
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

void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "filesystem" );
}

};
