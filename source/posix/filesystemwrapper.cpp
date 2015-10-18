#include <filesystemwrapper.hpp>
#include <filevalve.hpp>
#include <cstdint>
#include <cctype>
#include <unordered_set>
#include <algorithm>
#include <filesystem.h>
#include <strtools.h>
#include <unistd.h>

namespace filesystem
{

const size_t Wrapper::max_tempbuffer_len = 16384;
const std::unordered_set<std::string> Wrapper::whitelist_extensions = {
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
const std::unordered_set<std::string> Wrapper::whitelist_pathid[] = {
	{
		"data", "download", "lua", "lcl", "lsv", "game", "garrysmod", "gamebin", "mod",
		"base_path", "executable_path", "default_write_path"
	}, // Read
	{ "data", "download" }, // Write
	{ "game", "lcl" } // SearchPath
};
std::unordered_map<std::string, std::string> Wrapper::whitelist_writepaths;

Wrapper::Wrapper( ) :
	fsystem( nullptr )
{ }

Wrapper::~Wrapper( )
{ }

bool Wrapper::Initialize( IFileSystem *fsinterface )
{
	fsystem = fsinterface;

	{
		char fullpath[max_tempbuffer_len] = { 0 };
		int32_t len = fsystem->GetSearchPath_safe( "DEFAULT_WRITE_PATH", false, fullpath ) - 1;
		if( len <= 0 )
			return false;

		garrysmod_fullpath.assign( fullpath, 0, len );
	}

	return true;
}

file::Base *Wrapper::Open( const std::string &fpath, const std::string &opts, const std::string &pid )
{
	std::string filepath = fpath, options = opts, pathid = pid;

	std::transform( options.begin( ), options.end( ), options.begin( ), std::tolower );
	WhitelistType wtype = options.find_first_of( "wa+" ) != options.npos ?
		WhitelistType::Write : WhitelistType::Read;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, wtype ) ||
		!FixupFilePath( filepath, pathid ) ||
		!VerifyFilePath( filepath, false, nonascii ) ||
		!VerifyExtension( filepath, wtype ) )
		return nullptr;

	FileHandle_t fh = fsystem->Open( filepath.c_str( ), options.c_str( ), pathid.c_str( ) );
	if( fh == nullptr )
		return nullptr;

	file::Base *f = new( std::nothrow ) file::Valve( fsystem, fh );
	if( f == nullptr )
		fsystem->Close( fh );

	return f;
}

bool Wrapper::Exists( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return fsystem->FileExists( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsDirectory( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetSize( const std::string &fpath, const std::string &pid )
{
	std::string filepath = fpath, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filepath, pathid, WhitelistType::Read, nonascii ) )
		return 0;

	return fsystem->Size( filepath.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetTime( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return fsystem->GetPathTime( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Rename( const std::string &pold, const std::string &pnew, const std::string &pid )
{
	std::string pathold = pold, pathnew = pnew, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( pathold, pathid, WhitelistType::Write, nonascii ) ||
		!IsPathAllowed( pathnew, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( fsystem->IsDirectory( pathold.c_str( ), pathid.c_str( ) ) )
	{
		char fullpathold[max_tempbuffer_len] = { 0 };
		fsystem->RelativePathToFullPath_safe( pathold.c_str( ), pathid.c_str( ), fullpathold );
		char fullpathnew[max_tempbuffer_len] = { 0 };
		fsystem->RelativePathToFullPath_safe( pathnew.c_str( ), pathid.c_str( ), fullpathnew );
		return rename( fullpathold, fullpathnew ) == 0;
	}

	return fsystem->RenameFile( pathold.c_str( ), pathnew.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Remove( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;
	
	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) ) )
	{
		char fullpath[max_tempbuffer_len] = { 0 };
		fsystem->RelativePathToFullPath( path.c_str( ), pathid.c_str( ), fullpath, sizeof( fullpath ) );
		return rmdir( fullpath ) == 0;
	}

	if( fsystem->FileExists( path.c_str( ), pathid.c_str( ) ) )
	{
		fsystem->RemoveFile( path.c_str( ), pathid.c_str( ) );
		return !fsystem->FileExists( path.c_str( ), pathid.c_str( ) );
	}

	return false;
}

bool Wrapper::MakeDirectory( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Write, nonascii ) )
		return false;

	fsystem->CreateDirHierarchy( path.c_str( ), pathid.c_str( ) );
	return fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

std::pair< std::set<std::string>, std::set<std::string> > Wrapper::Find(
	const std::string &p,
	const std::string &pid
)
{
	std::string filename = p, pathid = pid;

	std::set<std::string> files, directories;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read, nonascii, true ) )
		return std::make_pair( files, directories );

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *path = fsystem->FindFirstEx( filename.c_str( ), pathid.c_str( ), &handle );
	while( path != nullptr )
	{
		if( fsystem->FindIsDirectory( handle ) )
			directories.insert( path );
		else
			files.insert( path );

		path = fsystem->FindNext( handle );
	}

	fsystem->FindClose( handle );
	return std::make_pair( files, directories );
}

std::list<std::string> Wrapper::GetSearchPaths( const std::string &pathid )
{
	std::list<std::string> searchpaths;

	char paths[max_tempbuffer_len] = { 0 };
	int32_t len = fsystem->GetSearchPath_safe( pathid.c_str( ), true, paths ) - 1;
	if( len <= 0 )
		return searchpaths;

	char *start = paths, *end = paths + len, *pos = std::find( start, end, ';' );
	for( ; pos != end; start = ++pos, pos = std::find( start, end, ';' ) )
	{
		*pos = '\0';
		searchpaths.push_back( start );
	}

	if( start != end )
		searchpaths.push_back( start );

	return searchpaths;
}

bool Wrapper::AddSearchPath( const std::string &p, const std::string &pid )
{
	std::string directory = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::SearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false, nonascii ) )
		return false;

	directory.insert( 0, garrysmod_fullpath );
	fsystem->AddSearchPath( directory.c_str( ), pathid.c_str( ) );
	return true;
}

bool Wrapper::RemoveSearchPath( const std::string &p, const std::string &pid )
{
	std::string directory = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::SearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false, nonascii ) )
		return false;

	directory.insert( 0, garrysmod_fullpath );
	return fsystem->RemoveSearchPath( directory.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsPathIDAllowed( std::string &pathid, WhitelistType whitelist_type )
{
	if( pathid.empty( ) )
		return false;

	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), std::tolower );
	const auto &whitelist = whitelist_pathid[static_cast<size_t>( whitelist_type )];
	return whitelist.find( pathid ) != whitelist.end( );
}

bool Wrapper::FixupFilePath( std::string &filepath, const std::string &pathid )
{
	if( filepath.empty( ) )
		return false;

	if( V_IsAbsolutePath( filepath.c_str( ) ) )
	{
		std::string tpath = filepath;
		filepath.resize( max_tempbuffer_len );
		if( !fsystem->FullPathToRelativePathEx( tpath.c_str( ), pathid.c_str( ), &filepath[0], filepath.size( ) ) )
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


bool Wrapper::VerifyFilePath( const std::string &filepath, bool find, bool &nonascii )
{
	nonascii = false;
	return true;
}

bool Wrapper::VerifyExtension( const std::string &filepath, WhitelistType whitelist_type )
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

bool Wrapper::IsPathAllowed(
	std::string &filepath,
	std::string &pathid,
	WhitelistType whitelist_type,
	bool &nonascii,
	bool find
)
{
	return FixupFilePath( filepath, pathid ) &&
		VerifyFilePath( filepath, find, nonascii ) &&
		VerifyExtension( filepath, whitelist_type );
}

std::string Wrapper::GetPath(
	const std::string &filepath,
	const std::string &pathid,
	WhitelistType wtype
)
{
	return "";
}

}
