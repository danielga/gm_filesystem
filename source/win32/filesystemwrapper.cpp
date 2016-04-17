#include <filesystemwrapper.hpp>
#include <filevalve.hpp>
#include "filestream.hpp"
#include "unicode.hpp"
#include <cstdint>
#include <cstring>
#include <cctype>
#include <unordered_set>
#include <algorithm>
#include <basefilesystem.hpp>
#include <utllinkedlist.h>
#include <strtools.h>
#include <direct.h>
#include <Windows.h>
#include <Shlobj.h>

#undef SearchPath

namespace filesystem
{

const size_t Wrapper::max_tempbuffer_len = 16384;
std::unordered_set<std::string> Wrapper::whitelist_extensions;
std::unordered_set<std::string> Wrapper::whitelist_pathid[3];
std::unordered_map<std::string, std::string> Wrapper::whitelist_writepaths;

// BAD WINDOWS, YOU KNOTTY BOY
static std::unordered_set<uint32_t> blacklist_characters;
static std::unordered_set<std::string> blacklist_filenames;

Wrapper::Wrapper( ) :
	fsystem( nullptr )
{
	if( whitelist_extensions.empty( ) )
	{
		const std::string extensions[] = {
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
		whitelist_extensions.insert( extensions, extensions + sizeof( extensions ) / sizeof( *extensions ) );
	}

	if( whitelist_pathid[0].empty( ) )
	{
		const std::string pathids0[] = {
			"data", "download", "lua", "lcl", "lsv", "game", "garrysmod", "gamebin", "mod",
			"base_path", "executable_path", "default_write_path"
		}; // Read
		whitelist_pathid[0].insert( pathids0, pathids0 + sizeof( pathids0 ) / sizeof( *pathids0 ) );
	}

	if( whitelist_pathid[1].empty( ) )
	{
		const std::string pathids1[] = { "data", "download" }; // Write
		whitelist_pathid[1].insert( pathids1, pathids1 + sizeof( pathids1 ) / sizeof( *pathids1 ) );
	}

	if( whitelist_pathid[2].empty( ) )
	{
		const std::string pathids2[] = { "game", "lcl" }; // SearchPath
		whitelist_pathid[2].insert( pathids2, pathids2 + sizeof( pathids2 ) / sizeof( *pathids2 ) );
	}

	if( blacklist_characters.empty( ) )
	{
		const uint32_t characters[] = { '<', '>', ':', '"', '/', '|', '?' };
		blacklist_characters.insert( characters, characters + sizeof( characters ) / sizeof( *characters ) );
	}

	if( blacklist_filenames.empty( ) )
	{
		const std::string filenames[] = {
			"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
			"COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
		};
		blacklist_filenames.insert( filenames, filenames + sizeof( filenames ) / sizeof( *filenames ) );
	}
}

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

	// this relies on the fact that all the writable pathids on the whitelist have a single path
	if( whitelist_writepaths.empty( ) )
	{
		char searchpath[max_tempbuffer_len] = { 0 };
		const std::unordered_set<std::string> &whitelist = whitelist_pathid[static_cast<size_t>( WhitelistWrite )];
		for( auto it = whitelist.begin( ); it != whitelist.end( ); ++it )
		{
			int32_t len = fsystem->GetSearchPath_safe( ( *it ).c_str( ), false, searchpath ) - 1;
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

	std::transform( options.begin( ), options.end( ), options.begin( ), std::tolower );
	WhitelistType wtype = options.find_first_of( "wa+" ) != options.npos ?
		WhitelistWrite : WhitelistRead;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, wtype ) ||
		!FixupFilePath( filepath, pathid ) ||
		!VerifyFilePath( filepath, false, nonascii ) ||
		!VerifyExtension( filepath, wtype ) )
		return nullptr;

	if( nonascii )
	{
		filepath = GetPath( filepath, pathid, wtype );
		const std::wstring wfilename = Unicode::UTF8::ToUTF16( filepath.begin( ), filepath.end( ) );
		const std::wstring woptions = Unicode::UTF8::ToUTF16( options.begin( ), options.end( ) );
		FILE *fh = _wfopen( wfilename.c_str( ), woptions.c_str( ) );
		if( fh == nullptr )
			return nullptr;

		file::Base *f = new( std::nothrow ) file::Stream( fh );
		if( f == nullptr )
			fclose( fh );

		return f;
	}

	FileHandle_t fh = fsystem->Open( filepath.c_str( ), options.c_str( ), pathid.c_str( ) );
	if( fh == nullptr )
		return nullptr;

	file::Base *f = new( std::nothrow ) file::Valve( reinterpret_cast<IFileSystem *>( fsystem ), fh );
	if( f == nullptr )
		fsystem->Close( fh );

	return f;
}

bool Wrapper::Exists( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistRead );
		std::wstring wpath = Unicode::UTF8::ToUTF16( path.begin( ), path.end( ) );
		return GetFileAttributesW( wpath.c_str( ) ) != INVALID_FILE_ATTRIBUTES;
	}

	return fsystem->FileExists( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsDirectory( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistRead );
		std::wstring wpath = Unicode::UTF8::ToUTF16( path.begin( ), path.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return false;

		return ( file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
	}

	return fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetSize( const std::string &fpath, const std::string &pid ) const
{
	std::string filepath = fpath, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( filepath, pathid, WhitelistRead, nonascii ) )
		return 0;

	if( nonascii )
	{
		filepath = GetPath( filepath, pathid, WhitelistRead );
		std::wstring wpath = Unicode::UTF8::ToUTF16( filepath.begin( ), filepath.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return 0;

		uint64_t high = static_cast<uint64_t>( file_data.nFileSizeHigh ),
			low = static_cast<uint64_t>( file_data.nFileSizeLow );
		return ( high << 32 ) | low;
	}

	return fsystem->Size( filepath.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetTime( const std::string &p, const std::string &pid ) const
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistRead ) ||
		!IsPathAllowed( path, pathid, WhitelistRead, nonascii ) )
		return 0;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistRead );
		const std::wstring wpath = Unicode::UTF8::ToUTF16( path.begin( ), path.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return 0;

		const uint64_t high = static_cast<uint64_t>( file_data.ftLastWriteTime.dwHighDateTime ),
			low = static_cast<uint64_t>( file_data.ftLastWriteTime.dwLowDateTime );
		return ( high << 32 ) | low;
	}

	return fsystem->GetPathTime( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Rename( const std::string &pold, const std::string &pnew, const std::string &pid )
{
	std::string pathold = pold, pathnew = pnew, pathid = pid;

	bool nonasciio = false, nonasciin = false;
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( pathold, pathid, WhitelistWrite, nonasciio ) ||
		!IsPathAllowed( pathnew, pathid, WhitelistWrite, nonasciin ) )
		return false;

	if( nonasciio || nonasciin )
	{
		pathold = GetPath( pathold, pathid, WhitelistWrite );
		pathnew = GetPath( pathnew, pathid, WhitelistWrite );
		const std::wstring wpathold = Unicode::UTF8::ToUTF16( pathold.begin( ), pathold.end( ) );
		const std::wstring wpathnew = Unicode::UTF8::ToUTF16( pathnew.begin( ), pathnew.end( ) );
		return MoveFileW( wpathold.c_str( ), wpathnew.c_str( ) ) == 1;
	}

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
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( path, pathid, WhitelistWrite, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistWrite );
		const std::wstring wpath = Unicode::UTF8::ToUTF16( path.begin( ), path.end( ) );
		return RemoveDirectoryW( wpath.c_str( ) ) == 1;
	}

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
	if( !IsPathIDAllowed( pathid, WhitelistWrite ) ||
		!IsPathAllowed( path, pathid, WhitelistWrite, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistWrite );
		const std::wstring wpath = Unicode::UTF8::ToUTF16( path.begin( ), path.end( ) );
		return SHCreateDirectoryExW( nullptr, wpath.c_str( ), nullptr ) == ERROR_SUCCESS;
	}

	fsystem->CreateDirHierarchy( path.c_str( ), pathid.c_str( ) );
	return fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

// this function is problematic
// valve's IFileSystem does some weird stuff with unicode paths/names
// Win32 API doesn't search inside VPKs and GMAs (obviously)
// combining them will duplicate results (broken names so not exactly duplicating)
// besides doing the search twice on each path
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

	// IFileSystem finding (allows finding inside VPKs, GMAs and what not)
	{
		FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
		const char *path = fsystem->FindFirstEx( filename.c_str( ), pathid.c_str( ), &handle );
		if( handle != FILESYSTEM_INVALID_FIND_HANDLE )
		{
			while( path != nullptr )
			{
				if( fsystem->FindIsDirectory( handle ) )
				{
					if( std::strcmp( path, "." ) != 0 && std::strcmp( path, ".." ) != 0 )
						directories.insert( path );
				}
				else
					files.insert( path );

				path = fsystem->FindNext( handle );
			}

			fsystem->FindClose( handle );
		}
	}

	// Win32 API finding (allows proper handling of Unicode)
	{
		std::list<std::string> searchpaths = GetSearchPaths( pathid );
		for( auto it = searchpaths.begin( ); it != searchpaths.end( ); ++it )
		{
			// check if the searchpath is actually a file, we can search inside these
			const DWORD attrib = GetFileAttributesA( ( *it ).c_str( ) );
			if( attrib != INVALID_FILE_ATTRIBUTES && ( attrib & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
				continue;

			char fullpath[max_tempbuffer_len] = { 0 };
			V_ComposeFileName( ( *it ).c_str( ), filename.c_str( ), fullpath, sizeof( fullpath ) );
			const std::wstring wfilename = Unicode::UTF8::ToUTF16( fullpath, fullpath + std::strlen( fullpath ) );

			WIN32_FIND_DATAW find_data;
			HANDLE handle = FindFirstFileExW(
				wfilename.c_str( ),
				FindExInfoStandard,
				&find_data,
				FindExSearchNameMatch,
				nullptr,
				0
			);
			if( handle == INVALID_HANDLE_VALUE )
				continue;

			do
			{
				const std::wstring path = find_data.cFileName;
				if( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				{
					if( path.compare( L"." ) != 0 && path.compare( L".." ) != 0 )
						directories.insert( Unicode::UTF16::ToUTF8( path.begin( ), path.end( ) ) );
				}
				else
				{
					files.insert( Unicode::UTF16::ToUTF8( path.begin( ), path.end( ) ) );
				}
			}
			while( FindNextFileW( handle, &find_data ) );

			FindClose( handle );
		}
	}

	return std::make_pair( files, directories );
}

std::unordered_map< std::string, std::list<std::string> > Wrapper::GetSearchPaths( ) const
{
	CBaseFileSystem *fsystem = reinterpret_cast<CBaseFileSystem *>( this->fsystem );

	std::unordered_map< std::string, std::list<std::string> > searchpaths;

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
			searchpaths[m_pPathIDInfo->m_pDebugPathID].push_back( filepath );
		}
		else
			searchpaths[m_pPathIDInfo->m_pDebugPathID].push_back( searchpath.m_pDebugPath );
	}

	return searchpaths;
}

std::list<std::string> Wrapper::GetSearchPaths( const std::string &pathid ) const
{
	std::list<std::string> searchpaths;

	char paths[max_tempbuffer_len] = { 0 };
	const int32_t len = fsystem->GetSearchPath_safe( pathid.c_str( ), true, paths ) - 1;
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
	if( !IsPathIDAllowed( pathid, WhitelistSearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false, nonascii ) ||
		nonascii )
		return false;

	directory.insert( 0, garrysmod_fullpath );
	fsystem->AddSearchPath( directory.c_str( ), pathid.c_str( ), PATH_ADD_TO_TAIL );
	return true;
}

bool Wrapper::RemoveSearchPath( const std::string &p, const std::string &pid )
{
	std::string directory = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistSearchPath ) ||
		!FixupFilePath( directory, "DEFAULT_WRITE_PATH" ) ||
		!VerifyFilePath( directory, false, nonascii ) ||
		nonascii )
		return false;

	directory.insert( 0, garrysmod_fullpath );
	return fsystem->RemoveSearchPath( directory.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsPathIDAllowed( std::string &pathid, WhitelistType whitelist_type ) const
{
	if( pathid.empty( ) )
		return false;

	std::transform( pathid.begin( ), pathid.end( ), pathid.begin( ), std::tolower );
	const std::unordered_set<std::string> &whitelist = whitelist_pathid[static_cast<size_t>( whitelist_type )];
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


bool Wrapper::VerifyFilePath( const std::string &filepath, bool find, bool &nonascii ) const
{
	nonascii = false;

	auto begin = filepath.begin( ), end = filepath.end( );
	uint32_t out = 0;
	do
	{
		begin = Unicode::UTF8::Decode( begin, end, out );
		if( out >= 128 )
			nonascii = true;

		if( out < 32 || ( !find && out == '*' || blacklist_characters.find( out ) != blacklist_characters.end( ) ) )
			return false;
	}
	while( begin != end );

	const char *filename = V_GetFileName( filepath.c_str( ) );
	std::string filename_extless( filename, 0, 4 );
	if( filename_extless.size( ) >= 3 )
	{
		if( filename_extless.size( ) >= 4 && filename_extless[3] == '.' )
			filename_extless.resize( 3 );

		if( blacklist_filenames.find( filename_extless ) != blacklist_filenames.end( ) )
			return false;
	}

	return true;
}

bool Wrapper::VerifyExtension( const std::string &filepath, WhitelistType whitelist_type ) const
{
	const char *extension = V_GetFileExtension( filepath.c_str( ) );
	if( whitelist_type == WhitelistWrite && extension != nullptr )
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
	if( wtype == WhitelistRead )
	{
		bool notfound = true;
		const std::list<std::string> searchpaths = GetSearchPaths( pathid );
		for( auto it = searchpaths.begin( ); it != searchpaths.end( ); ++it )
		{
			V_ComposeFileName( ( *it ).c_str( ), filepath.c_str( ), fullpath, sizeof( fullpath ) );
			const std::wstring wfilename = Unicode::UTF8::ToUTF16( fullpath, fullpath + std::strlen( fullpath ) );
			if( GetFileAttributesW( wfilename.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
			{
				notfound = false;
				break;
			}
		}

		if( notfound )
			return "";
	}
	else if( wtype == WhitelistWrite )
	{
		const auto searchpath = whitelist_writepaths.find( pathid );
		if( searchpath == whitelist_writepaths.end( ) )
			return "";

		V_ComposeFileName(
			( *searchpath ).second.c_str( ),
			filepath.c_str( ),
			fullpath,
			sizeof( fullpath )
		);
	}

	return fullpath;
}

}