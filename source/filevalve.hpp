#pragma once

#include "filebase.hpp"

typedef void *FileHandle_t;
class CBaseFileSystem;

namespace file
{
	
class Valve : public Base
{
public:
	Valve( CBaseFileSystem *fsystem, FileHandle_t handle );
	~Valve( );

	bool Valid( ) const;
	bool Good( ) const;
	bool EndOfFile( ) const;

	bool Close( );

	int64_t Size( ) const;
	int64_t Tell( ) const;
	bool Seek( int64_t pos, SeekDirection dir );

	bool Flush( );

	size_t Read( void *buffer, size_t len );
	size_t Write( const void *buffer, size_t len );

private:
	CBaseFileSystem *filesystem;
	FileHandle_t filehandle;
};

}
