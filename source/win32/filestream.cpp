#include "filestream.hpp"

namespace file
{

Stream::Stream( FILE *handle ) :
	filehandle( handle )
{ }

Stream::~Stream( )
{
	Close( );
}

bool Stream::Valid( ) const
{
	return filehandle != nullptr;
}

bool Stream::Good( ) const
{
	if( !Valid( ) )
		return true;

	return ferror( filehandle ) == 0;
}

bool Stream::EndOfFile( ) const
{
	if( !Valid( ) )
		return true;

	return feof( filehandle ) != 0;
}

bool Stream::Close( )
{
	if( !Valid( ) )
		return false;

	fclose( filehandle );
	filehandle = nullptr;
	return true;
}

int64_t Stream::Size( ) const
{
	if( !Valid( ) )
		return -1;

	int64_t pos = _ftelli64( filehandle );
	_fseeki64( filehandle, 0, SEEK_END );
	int64_t size = _ftelli64( filehandle );
	_fseeki64( filehandle, pos, SEEK_SET );
	return size;
}

int64_t Stream::Tell( ) const
{
	if( !Valid( ) )
		return -1;

	return _ftelli64( filehandle );
}

bool Stream::Seek( int64_t pos, SeekDirection dir )
{
	if( !Valid( ) )
		return false;

	return _fseeki64( filehandle, pos, static_cast<int32_t>( dir ) ) == 0;
}

bool Stream::Flush( )
{
	if( !Valid( ) )
		return false;

	return fflush( filehandle ) == 0;
}

size_t Stream::Read( void *buffer, size_t len )
{
	if( !Valid( ) )
		return 0;

	return fread( buffer, 1, len, filehandle );
}

size_t Stream::Write( const void *buffer, size_t len )
{
	if( !Valid( ) )
		return false;

	return fwrite( buffer, 1, len, filehandle );
}

}
