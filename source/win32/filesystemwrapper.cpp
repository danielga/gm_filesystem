#include <filesystemwrapper.hpp>
#include <filevalve.hpp>
#include <filestream.hpp>
#include <cstdint>
#include <cctype>
#include <unordered_set>
#include <algorithm>
#include <filesystem.h>
#include <strtools.h>
#include <iterator>
#include <direct.h>
#include <Windows.h>
#include <Shlobj.h>

#undef SearchPath

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

template<typename Output>
inline Output UTF16Encode( uint32_t input, Output output, uint16_t replace = 0 )
{
	if( input < 0xFFFF )
	{
		if( input >= 0xD800 && input <= 0xDFFF )
		{
			if( replace )
			{
				*output = replace;
				++output;
			}
		}
		else
		{
			*output = static_cast<uint16_t>( input );
			++output;
		}
	}
	else if( input > 0x0010FFFF )
	{
		if( replace )
		{
			*output = replace;
			++output;
		}
	}
	else
	{
		input -= 0x0010000;
		*output = static_cast<uint16_t>( ( input >> 10 ) + 0xD800 );
		++output;
		*output = static_cast<uint16_t>( ( input & 0x3FFUL ) + 0xDC00 );
		++output;
	}

	return output;
}

template<typename Input>
inline std::wstring UTF8ToUTF16( Input begin, Input end )
{
	std::wstring out;
	out.reserve( end - begin );
	auto inserter = std::back_inserter( out );

	uint32_t codepoint = 0;
	while( begin != end )
	{
		begin = UTF8Decode( begin, end, codepoint );
		inserter = UTF16Encode( codepoint, inserter );
	}

	return out;
}

template<typename Input>
inline Input UTF16Decode( Input begin, Input end, uint32_t &output, uint32_t replace = 0 )
{
	uint16_t first = *begin;
	++begin;

	if( first >= 0xD800 && first <= 0xDBFF )
	{
		if( begin < end )
		{
			uint32_t second = *begin;
			++begin;
			if( second >= 0xDC00 && second <= 0xDFFF )
				output = ( ( first - 0xD800 ) << 10 ) + second - 0xDC00 + 0x00010000;
			else
				output = replace;
		}
		else
		{
			begin = end;
			output = replace;
		}
	}
	else
	{
		output = first;
	}

	return begin;
}

template<typename Output>
inline Output UTF8Encode( uint32_t input, Output output, uint8_t replace = 0 )
{
	static const uint8_t firstBytes[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

	if( input > 0x0010FFFF || ( input >= 0xD800 && input <= 0xDBFF ) )
	{
		if( replace )
		{
			*output = replace;
			++output;
		}
	}
	else
	{
		size_t bytestoWrite = 1;
		if( input < 0x80 )
			bytestoWrite = 1;
		else if( input < 0x800 )
			bytestoWrite = 2;
		else if( input < 0x10000 )
			bytestoWrite = 3;
		else if( input <= 0x0010FFFF )
			bytestoWrite = 4;

		uint8_t bytes[4];
		switch( bytestoWrite )
		{
		case 4:
			bytes[3] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 3:
			bytes[2] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 2:
			bytes[1] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 1:
			bytes[0] = static_cast<uint8_t>( input | firstBytes[bytestoWrite] );
		}

		output = std::copy( bytes, bytes + bytestoWrite, output );
	}

	return output;
}

template<typename Input>
inline std::string UTF16ToUTF8( Input begin, Input end )
{
	std::string out;
	out.reserve( end - begin );
	auto inserter = std::back_inserter( out );

	uint32_t codepoint = 0;
	while( begin != end )
	{
		begin = UTF16Decode( begin, end, codepoint );
		inserter = UTF8Encode( codepoint, inserter );
	}

	return out;
}

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

	// this relies on the fact that all the writable pathids on the whitelist have a single path
	if( whitelist_writepaths.empty( ) )
	{
		char searchpath[max_tempbuffer_len] = { 0 };
		for( const std::string &it : whitelist_pathid[static_cast<size_t>( WhitelistType::Write )] )
		{
			int32_t len = fsystem->GetSearchPath_safe( it.c_str( ), false, searchpath ) - 1;
			if( len <= 0 )
				return false;

			whitelist_writepaths[it].assign( searchpath, 0, len );
		}
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

	if( nonascii )
	{
		filepath = GetPath( filepath, pathid, wtype );
		std::wstring wfilename = UTF8ToUTF16( filepath.begin( ), filepath.end( ) );
		std::wstring woptions = UTF8ToUTF16( options.begin( ), options.end( ) );
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

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistType::Read );
		std::wstring wpath = UTF8ToUTF16( path.begin( ), path.end( ) );
		return GetFileAttributesW( wpath.c_str( ) ) != INVALID_FILE_ATTRIBUTES;
	}

	return fsystem->FileExists( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::IsDirectory( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistType::Read );
		std::wstring wpath = UTF8ToUTF16( path.begin( ), path.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return false;

		return ( file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
	}

	return fsystem->IsDirectory( path.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetSize( const std::string &fpath, const std::string &pid )
{
	std::string filepath = fpath, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filepath, pathid, WhitelistType::Read, nonascii ) )
		return 0;

	if( nonascii )
	{
		filepath = GetPath( filepath, pathid, WhitelistType::Read );
		std::wstring wpath = UTF8ToUTF16( filepath.begin( ), filepath.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return 0;

		uint64_t high = static_cast<uint64_t>( file_data.nFileSizeHigh ),
			low = static_cast<uint64_t>( file_data.nFileSizeLow );
		return ( high << 32 ) | low;
	}

	return fsystem->Size( filepath.c_str( ), pathid.c_str( ) );
}

uint64_t Wrapper::GetTime( const std::string &p, const std::string &pid )
{
	std::string path = p, pathid = pid;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Read, nonascii ) )
		return 0;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistType::Read );
		std::wstring wpath = UTF8ToUTF16( path.begin( ), path.end( ) );
		WIN32_FILE_ATTRIBUTE_DATA file_data;
		if( !GetFileAttributesExW( wpath.c_str( ), GetFileExInfoStandard, &file_data ) )
			return 0;

		uint64_t high = static_cast<uint64_t>( file_data.ftLastWriteTime.dwHighDateTime ),
			low = static_cast<uint64_t>( file_data.ftLastWriteTime.dwLowDateTime );
		return ( high << 32 ) | low;
	}

	return fsystem->GetPathTime( path.c_str( ), pathid.c_str( ) );
}

bool Wrapper::Rename( const std::string &pold, const std::string &pnew, const std::string &pid )
{
	std::string pathold = pold, pathnew = pnew, pathid = pid;

	bool nonasciio = false, nonasciin = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( pathold, pathid, WhitelistType::Write, nonasciio ) ||
		!IsPathAllowed( pathnew, pathid, WhitelistType::Write, nonasciin ) )
		return false;

	if( nonasciio || nonasciin )
	{
		pathold = GetPath( pathold, pathid, WhitelistType::Write );
		pathnew = GetPath( pathnew, pathid, WhitelistType::Write );
		std::wstring wpathold = UTF8ToUTF16( pathold.begin( ), pathold.end( ) );
		std::wstring wpathnew = UTF8ToUTF16( pathnew.begin( ), pathnew.end( ) );
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistType::Write );
		std::wstring wpath = UTF8ToUTF16( path.begin( ), path.end( ) );
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
	if( !IsPathIDAllowed( pathid, WhitelistType::Write ) ||
		!IsPathAllowed( path, pathid, WhitelistType::Write, nonascii ) )
		return false;

	if( nonascii )
	{
		path = GetPath( path, pathid, WhitelistType::Write );
		std::wstring wpath = UTF8ToUTF16( path.begin( ), path.end( ) );
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
)
{
	std::string filename = p, pathid = pid;

	std::set<std::string> files, directories;

	bool nonascii = false;
	if( !IsPathIDAllowed( pathid, WhitelistType::Read ) ||
		!IsPathAllowed( filename, pathid, WhitelistType::Read, nonascii, true ) )
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
		for( const std::string &searchpath : searchpaths )
		{
			// check if the searchpath is actually a file, we can search inside these
			DWORD attrib = GetFileAttributesA( searchpath.c_str( ) );
			if( attrib != INVALID_FILE_ATTRIBUTES && ( attrib & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
				continue;

			char fullpath[max_tempbuffer_len] = { 0 };
			V_ComposeFileName( searchpath.c_str( ), filename.c_str( ), fullpath, sizeof( fullpath ) );
			std::wstring wfilename = UTF8ToUTF16( fullpath, fullpath + std::strlen( fullpath ) );

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
				std::wstring path = find_data.cFileName;
				if( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				{
					if( path.compare( L"." ) != 0 && path.compare( L".." ) != 0 )
						directories.insert( UTF16ToUTF8( path.begin( ), path.end( ) ) );
				}
				else
				{
					files.insert( UTF16ToUTF8( path.begin( ), path.end( ) ) );
				}
			}
			while( FindNextFileW( handle, &find_data ) );

			FindClose( handle );
		}
	}

	return std::make_pair( files, directories );
}

std::list<std::string> Wrapper::GetSearchPaths( const std::string &pathid )
{
	std::list<std::string> searchpaths;

	char paths[max_tempbuffer_len] = { 0 };
	int32_t len = fsystem->GetSearchPath( pathid.c_str( ), true, paths, sizeof( paths ) ) - 1;
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
		!VerifyFilePath( directory, false, nonascii ) ||
		nonascii )
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
		!VerifyFilePath( directory, false, nonascii ) ||
		nonascii )
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

	// BAD WINDOWS, YOU KNOTTY BOY
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
		static const std::unordered_set<std::string> blacklist_filenames = {
			"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
			"COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
		};

		if( filename_extless.size( ) >= 4 && filename_extless[3] == '.' )
			filename_extless.resize( 3 );

		if( blacklist_filenames.find( filename_extless ) != blacklist_filenames.end( ) )
			return false;
	}

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
	char fullpath[max_tempbuffer_len] = { 0 };
	if( wtype == WhitelistType::Read )
	{
		bool notfound = true;
		std::list<std::string> searchpaths = GetSearchPaths( pathid );
		for( const std::string &searchpath : searchpaths )
		{
			V_ComposeFileName( searchpath.c_str( ), filepath.c_str( ), fullpath, sizeof( fullpath ) );
			std::wstring wfilename = UTF8ToUTF16( fullpath, fullpath + std::strlen( fullpath ) );
			if( GetFileAttributesW( wfilename.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
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
		auto searchpath = whitelist_writepaths.find( pathid );
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
