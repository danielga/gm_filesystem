#pragma once

#include <filebase.hpp>
#include <stdio.h>

namespace file
{
	
class Stream : public Base
{
public:
	Stream( FILE *handle );
	~Stream( );

	bool Valid( ) const;
	bool Good( ) const;
	bool EndOfFile( ) const;

	bool Close( );

	int64_t Size( );
	int64_t Tell( );
	bool Seek( int64_t pos, SeekDirection dir );

	bool Flush( );

	size_t Read( void *buffer, size_t len );
	size_t Write( const void *buffer, size_t len );

private:
	FILE *filehandle;
};

}
