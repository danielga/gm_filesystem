#include "filesystemwrapper.hpp"
#include "filevalve.hpp"

#include <filesystem_base.h>

#include <cstring>
#include <cctype>
#include <algorithm>

#include <unistd.h>
#include <sys/stat.h>

namespace filesystem
{

const size_t Wrapper::max_tempbuffer_len = 2048;
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
	filesystem( nullptr )
{ }

Wrapper::~Wrapper( )
{ }

bool Wrapper::Initialize( CBaseFileSystem *fsinterface )
{
	filesystem = fsinterface;

	{
		char fullpath[max_tempbuffer_len] = { 0 };
		int32_t len = filesystem->GetSearchPath_safe( "DEFAULT_WRITE_PATH", false, fullpath ) - 1;
		if( len <= 0 )
			return false;

		garrysmod_fullpath.assign( fullpath, 0, len );
	}

	// this relies on the fact that all the writable pathids on the whitelist have a single path
	if( whitelist_writepaths.empty( ) )
	{
		char searchpath[max_tempbuffer_len] = { 0 };
		const std::unordered_set<std::string> &whitelist = whitelist_pathid[static_cast<size_t>( WhitelistType::Write )];
		for( auto it = whitelist.begin( ); it != whitelist.end( ); ++it )
		{
			int32_t len = filesystem->GetSearchPath_safe( it->c_str( ), false, searchpath ) - 1;
			if( len <= 0 )
				return false;

			whitelist_writepaths[*it].assign( searchpath, 0, len );
		}
	}

	return true;
}

file::Base *Wrapper::Open( const std::string &fpath, const std::string &opts, const std::string &pid )
{
	std::string filepath = fpath, options = opts, pathid = pid;

	std::transform( options.begin( ), options.end( ), options.begin( ), tolower );
	WhitelistType wtype = options.find_first_of( "wa+" ) != options.npos ?
		WhitelistType::Write : WhitelistType::Read;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, wtype ) ||
		!FixupFilePath( filepath, pathid ) ||
		!VerifyFilePath( filepath, false, nonascii ) ||
		!VerifyExtension( filepath, wtype ) )
		return nullptr;

	FileHandle_t fh = filesystem->Open( filepath.c_str( ), options.c_str( ), pathid.c_str( ) );
	if( fh == nullptr )
		return nullptr;

	file::Base *f = new( std::nothrow ) file::Valve( filesystem, fh );
	if( f == nullptr )
		filesystem->Close( fh );

	return f;
}

bool Wrapper::Exists( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return filesystem->FileExists( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsDirectory( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return filesystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetSize( const std::string &fpath, const std::string &pid ) const
{
	std::string filepath = fpath, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filepath, pathid, WhitelistType::Read, nonascii ) )
		return 0;

	return filesystem->Size( filepath.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetTime( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	return filesystem->GetPathTime( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Rename( const std::string &pold, const std::string &pnew, const std::string &pid )
{
	std::string pathold = pold, pathnew = pnew, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( pathold, pathid, WhitelistType::Write, nonascii ) ||
		!IsPathAllowed( pathnew, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( filesystem->IsDirectory( pathold.c_str( ), pathid.c_str( ) ) )
	{
		char fullpathold[max_tempbuffer_len] = { 0 };
		filesystem->RelativePathToFullPath_safe( pathold.c_str( ), pathid.c_str( ), fullpathold );
		char fullpathnew[max_tempbuffer_len] = { 0 };
		filesystem->RelativePathToFullPath_safe( pathnew.c_str( ), pathid.c_str( ), fullpathnew );
		return rename( fullpathold, fullpathnew ) == 0;
	}

	return filesystem->RenameFile( pathold.c_str( ), pathnew.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Remove( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;
	
	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( filesystem->IsDirectory( path.c_str( ), pathid.c_str( ) ) )
	{
		char fullpath[max_tempbuffer_len] = { 0 };
		filesystem->RelativePathToFullPath( path.c_str( ), pathid.c_str( ), fullpath, sizeof( fullpath ) );
		return rmdir( fullpath ) == 0;
	}

	if( filesystem->FileExists( path.c_str( ), pathid.c_str( ) ) )
	{
		filesystem->RemoveFile( path.c_str( ), pathid.c_str( ) );
		return !filesystem->FileExists( path.c_str( ), pathid.c_str( ) );
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

	filesystem->CreateDirHierarchy( path.c_str( ), pathid.c_str( ) );
	return filesystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

std::pair<std::set<std::string>, std::set<std::string>> Wrapper::Find(
	const std::string &p,
	const std::string &pid
) const
{
	std::string filename = p, pathid = pid;

	std::set<std::string> files, directories;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read, nonascii, true ) )
		return std::make_pair( files, directories );

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *path = filesystem->FindFirstEx( filename.c_str( ), pathid.c_str( ), &handle );
	while( path != nullptr )
	{
		if( filesystem->FindIsDirectory( handle ) )
			directories.insert( path );
		else
			files.insert( path );

		path = filesystem->FindNext( handle );
	}

	filesystem->FindClose( handle );
	return std::make_pair( files, directories );
}

std::unordered_map<std::string, std::set<std::string>> Wrapper::GetSearchPaths( ) const
{
	std::unordered_map<std::string, std::set<std::string>> searchpaths;

	const CUtlLinkedList<CBaseFileSystem::CSearchPath> &m_SearchPaths = filesystem->m_SearchPaths;
	for( uint16_t k = 0; k < m_SearchPaths.Count( ); ++k )
	{
		const CBaseFileSystem::CSearchPath &searchpath = m_SearchPaths[k];
		const CBaseFileSystem::CPathIDInfo *pathIDInfo = searchpath.m_pPathIDInfo;
		if( searchpath.m_pDebugPath == nullptr ||
			pathIDInfo == nullptr ||
			pathIDInfo->m_pDebugPathID == nullptr )
			continue;

		const auto packFile = searchpath.m_pPackFile;
		const auto packFile2 = searchpath.m_pPackFile2;
		if( packFile != nullptr )
			searchpaths[pathIDInfo->m_pDebugPathID].insert( packFile->m_ZipName.Get( ) );
		else if( packFile2 != nullptr )
			searchpaths[pathIDInfo->m_pDebugPathID].insert( packFile2->m_pszFullPathName );
		else
			searchpaths[pathIDInfo->m_pDebugPathID].insert( searchpath.m_pDebugPath );
	}

	return searchpaths;
}

std::set<std::string> Wrapper::GetSearchPaths( const std::string &pathid ) const
{
	std::set<std::string> searchpaths;

	char paths[max_tempbuffer_len] = { 0 };
	const int32_t len = filesystem->GetSearchPath_safe( pathid.c_str( ), true, paths ) - 1;
	if( len <= 0 )
		return searchpaths;

	char *start = paths, *end = paths + len, *pos = std::find( start, end, ';' );
	for( ; pos != end; start = ++pos, pos = std::find( start, end, ';' ) )
	{
		*pos = '\0';
		searchpaths.insert( start );
	}

	if( start != end )
		searchpaths.insert( start );

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
	filesystem->AddSearchPath( directory.c_str( ), pathid.c_str( ), PATH_ADD_TO_TAIL );
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
	return filesystem->RemoveSearchPath( directory.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsPathIDAllowed( std::string &pathid, WhitelistType whitelist_type ) const
{
	if( pathid.empty( ) )
		return false;

	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), tolower );
	const auto &whitelist = whitelist_pathid[static_cast<size_t>( whitelist_type )];
	return whitelist.find( pathid ) != whitelist.end( );
}

bool Wrapper::FixupFilePath( std::string &filepath, const std::string &pathid ) const
{
	if( filepath.empty( ) )
		return false;

	if( V_IsAbsolutePath( filepath.c_str( ) ) )
	{
		const std::string tpath = filepath;
		filepath.resize( max_tempbuffer_len );
		if( !filesystem->FullPathToRelativePathEx( tpath.c_str( ), pathid.c_str( ), &filepath[0], filepath.size( ) ) )
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


bool Wrapper::VerifyFilePath( const std::string &filepath, bool find, bool &nonascii ) const
{
	nonascii = false;
	return true;
}

bool Wrapper::VerifyExtension( const std::string &filepath, WhitelistType whitelist_type ) const
{
	const char *extension = V_GetFileExtension( filepath.c_str( ) );
	if( whitelist_type == WhitelistType::Write && extension != nullptr )
	{
		std::string ext = extension;
		std::transform( ext.begin( ), ext.end( ), ext.begin( ), tolower );
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
) const
{
	return FixupFilePath( filepath, pathid ) &&
		VerifyFilePath( filepath, find, nonascii ) &&
		VerifyExtension( filepath, whitelist_type );
}

std::string Wrapper::GetPath(
	const std::string &filepath,
	const std::string &pathid,
	WhitelistType wtype
) const
{
	char fullpath[max_tempbuffer_len] = { 0 };
	if( wtype == WhitelistType::Read )
	{
		bool notfound = true;
		const std::set<std::string> searchpaths = GetSearchPaths( pathid );
		for( auto it = searchpaths.begin( ); it != searchpaths.end( ); ++it )
		{
			V_ComposeFileName( it->c_str( ), filepath.c_str( ), fullpath, sizeof( fullpath ) );
			struct stat stats;
			if( stat( filepath.c_str( ), &stats ) != -1 )
			{
				notfound = false;
				break;
			}
		}

		if( notfound )
			return "";
	}
	else if( wtype == WhitelistType::Write )
	{
		const auto searchpath = whitelist_writepaths.find( pathid );
		if( searchpath == whitelist_writepaths.end( ) )
			return "";

		V_ComposeFileName(
			searchpath->second.c_str( ),
			filepath.c_str( ),
			fullpath,
			sizeof( fullpath )
		);
	}

	return fullpath;
}

}
