#include <filevalve.hpp>
#include <filesystem.h>

namespace file
{

Valve::Valve( IFileSystem *fsystem, FileHandle_t handle ) :
	filesystem( fsystem ),
	filehandle( handle )
{ }

Valve::~Valve( )
{
	Close( );
}

bool Valve::Valid( ) const
{
	return filehandle != nullptr;
}

bool Valve::Good( ) const
{
	if( !Valid( ) )
		return true;

	return filesystem->IsOk( filehandle );
}

bool Valve::EndOfFile( ) const
{
	if( !Valid( ) )
		return true;

	return filesystem->EndOfFile( filehandle );
}

bool Valve::Close( )
{
	if( !Valid( ) )
		return false;

	filesystem->Close( filehandle );
	filehandle = nullptr;
	filesystem = nullptr;
	return true;
}

int64_t Valve::Size( ) const
{
	if( !Valid( ) )
		return -1;

	return filesystem->Size( filehandle );
}

int64_t Valve::Tell( ) const
{
	if( !Valid( ) )
		return -1;

	return filesystem->Tell( filehandle );
}

bool Valve::Seek( int64_t pos, SeekDirection dir )
{
	if( !Valid( ) || pos < -0x7FFFFFFF - 1 || pos > 0x7FFFFFFF )
		return false;

	filesystem->Seek( filehandle, static_cast<int32_t>( pos ), static_cast<FileSystemSeek_t>( dir ) );
	return true;
}

bool Valve::Flush( )
{
	if( !Valid( ) )
		return false;

	filesystem->Flush( filehandle );
	return true;
}

size_t Valve::Read( void *buffer, size_t len )
{
	if( !Valid( ) || len > 0x7FFFFFFF )
		return 0;

	return filesystem->Read( buffer, static_cast<int32_t>( len ), filehandle );
}

size_t Valve::Write( const void *buffer, size_t len )
{
	if( !Valid( ) || len > 0x7FFFFFFF )
		return false;

	return filesystem->Write( buffer, static_cast<int32_t>( len ), filehandle );
}

}
