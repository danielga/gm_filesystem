#include <filesystemwrapper.hpp>
#include <filevalve.hpp>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <basefilesystem.hpp>
#include <utllinkedlist.h>
#include <strtools.h>
#include <unistd.h>

namespace filesystem
{

const size_t Wrapper::max_tempbuffer_len = 16384;
std::unordered_set<std::string> Wrapper::whitelist_extensions = {
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
std::unordered_set<std::string> Wrapper::whitelist_pathid[] = {
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

bool Wrapper::Initialize( IFileSystem *fsinterface )
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
		const std::unordered_set<std::string> &whitelist = whitelist_pathid[static_cast<size_t>( WhitelistWrite )];
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
		WhitelistWrite : WhitelistRead;

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
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return false;

	return filesystem->FileExists( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsDirectory( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return false;

	return filesystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetSize( const std::string &fpath, const std::string &pid ) const
{
	std::string filepath = fpath, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( filepath, pathid, WhitelistRead, nonascii ) )
		return 0;

	return filesystem->Size( filepath.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetTime( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return false;

	return filesystem->GetPathTime( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Rename( const std::string &pold, const std::string &pnew, const std::string &pid )
{
	std::string pathold = pold, pathnew = pnew, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( pathold, pathid, WhitelistWrite, nonascii ) ||
		!IsPathAllowed( pathnew, pathid, WhitelistWrite, nonascii ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( path, pathid, WhitelistWrite, nonascii ) )
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
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( path, pathid, WhitelistWrite, nonascii ) )
		return false;

	filesystem->CreateDirHierarchy( path.c_str( ), pathid.c_str( ) );
	return filesystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

std::pair< std::set<std::string>, std::set<std::string> > Wrapper::Find(
	const std::string &p,
	const std::string &pid
) const
{
	std::string filename = p, pathid = pid;

	std::set<std::string> files, directories;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( filename, pathid, WhitelistRead, nonascii, true ) )
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

std::unordered_map< std::string, std::set<std::string> > Wrapper::GetSearchPaths( ) const
{
	CBaseFileSystem *fsystem = reinterpret_cast<CBaseFileSystem *>( this->filesystem );

	std::unordered_map< std::string, std::set<std::string> > searchpaths;

	const CUtlLinkedList<CBaseFileSystem::CSearchPath> &m_SearchPaths = fsystem->m_SearchPaths;
	for( int32_t k = 0; k < m_SearchPaths.Count( ); ++k )
	{
		const CBaseFileSystem::CSearchPath &searchpath = m_SearchPaths[k];
		const CBaseFileSystem::CPathIDInfo *m_pPathIDInfo = searchpath.m_pPathIDInfo;
		if( searchpath.m_pDebugPath == nullptr ||
			m_pPathIDInfo == nullptr ||
			m_pPathIDInfo->m_pDebugPathID == nullptr )
			continue;

		const WilloxHallOfShame *m_pPackFile = searchpath.m_pPackFile;
		if( m_pPackFile != nullptr )
		{
			std::string filepath = m_pPackFile->filepath;
			filepath += ".vpk";
			searchpaths[m_pPathIDInfo->m_pDebugPathID].insert( filepath );
		}
		else
			searchpaths[m_pPathIDInfo->m_pDebugPathID].insert( searchpath.m_pDebugPath );
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
	if( !IsPathIDAllowed( pathid, WhitelistSearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false, nonascii ) )
		return false;

	directory.insert( 0, garrysmod_fullpath );
	filesystem->AddSearchPath( directory.c_str( ), pathid.c_str( ) );
	return true;
}

bool Wrapper::RemoveSearchPath( const std::string &p, const std::string &pid )
{
	std::string directory = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistSearchPath ) ||
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
	if( whitelist_type == WhitelistWrite && extension != nullptr )
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
	return "";
}

}
