#pragma once

#include <cstdint>
#include <cstddef>

namespace file
{

enum class SeekDirection
{
	Set,
	Cur,
	End
};

class Base
{
public:
	virtual ~Base( ) { };

	virtual bool Valid( ) const = 0;
	virtual bool Good( ) const = 0;
	virtual bool EndOfFile( ) const = 0;

	virtual bool Close( ) = 0;

	virtual int64_t Size( ) = 0;
	virtual int64_t Tell( ) = 0;
	virtual bool Seek( int64_t pos, SeekDirection dir ) = 0;

	virtual bool Flush( ) = 0;

	virtual size_t Read( void *buffer, size_t len ) = 0;
	virtual size_t Write( const void *buffer, size_t len ) = 0;
};

}
