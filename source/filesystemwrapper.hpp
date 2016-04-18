#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <set>
#include <unordered_set>
#include <unordered_map>

class IFileSystem;
class CBaseFileSystem;

namespace file
{

class Base;

}

namespace filesystem
{

class Wrapper
{
public:
	Wrapper( );
	~Wrapper( );

	bool Initialize( IFileSystem *fsinterface );

	file::Base *Open(
		const std::string &filepath,
		const std::string &options,
		const std::string &pathid
	);

	bool Exists( const std::string &filepath, const std::string &pathid ) const;
	bool IsDirectory( const std::string &filepath, const std::string &pathid ) const;

	uint64_t GetSize( const std::string &filepath, const std::string &pathid ) const;
	uint64_t GetTime( const std::string &filepath, const std::string &pathid ) const;

	bool Rename( const std::string &pathold, const std::string &pathnew, const std::string &pathid );
	bool Remove( const std::string &path, const std::string &pathid );
	bool MakeDirectory( const std::string &path, const std::string &pathid );

	std::pair< std::set<std::string>, std::set<std::string> > Find(
		const std::string &path,
		const std::string &pathid
	) const;

	std::unordered_map< std::string, std::set<std::string> > GetSearchPaths( ) const;
	std::set<std::string> GetSearchPaths( const std::string &pathid ) const;
	bool AddSearchPath( const std::string &path, const std::string &pathid );
	bool RemoveSearchPath( const std::string &path, const std::string &pathid );

private:
	enum WhitelistType
	{
		WhitelistRead,
		WhitelistWrite,
		WhitelistSearchPath
	};

	bool IsPathIDAllowed( std::string &pathid, WhitelistType whitelist_type ) const;
	bool FixupFilePath( std::string &filepath, const std::string &pathid ) const;
	bool VerifyFilePath( const std::string &filepath, bool find, bool &nonascii ) const;
	bool VerifyExtension( const std::string &filepath, WhitelistType whitelist_type ) const;
	bool IsPathAllowed(
		std::string &filepath,
		std::string &pathid,
		WhitelistType whitelist_type,
		bool &nonascii,
		bool find = false
	) const;
	std::string GetPath(
		const std::string &filepath,
		const std::string &pathid,
		WhitelistType whitelist_type
	) const;

	static const size_t max_tempbuffer_len;
	static std::unordered_set<std::string> whitelist_extensions;
	static std::unordered_set<std::string> whitelist_pathid[];
	static std::unordered_map<std::string, std::string> whitelist_writepaths;

	IFileSystem *fsystem;
	std::string garrysmod_fullpath;
};

}
