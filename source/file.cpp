#include <GarrysMod/Lua/Interface.h>
#include <file.hpp>
#include <filebase.hpp>
#include <lua.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace file
{

static const char *metaname = "FileHandle";
static int32_t metatype = GarrysMod::Lua::Type::NONE;
static const char *invalid_error = "invalid FileHandle";

struct Container
{
	Base *file;
	bool invert;
};

inline void CheckType( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( LUA->GetState( ), index, metaname );
}

static Base *Get( GarrysMod::Lua::ILuaBase *LUA, int32_t index, bool *invert = nullptr )
{
	CheckType( LUA, index );
	Container *container = LUA->GetUserType<Container>( index, metatype );
	Base *file = container->file;
	if( file == nullptr )
		LUA->ArgError( index, invalid_error );

	if( invert != nullptr )
		*invert = container->invert;

	return file;
}

void Create( GarrysMod::Lua::ILuaBase *LUA, Base *file )
{
	Container *container = LUA->NewUserType<Container>( metatype );
	container->file = file;
	container->invert = false;

	LUA->PushMetaTable( metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( LUA->GetState( ), -2 );
}

template<class Type> inline Type InvertBytes( Type data, bool invert )
{
	if( invert )
	{
		uint8_t *start = reinterpret_cast<uint8_t *>( &data ), *end = start + sizeof( Type ) - 1;
		for( size_t k = 0; k < sizeof( Type ) / 2; ++k )
			std::swap( *start++, *end-- );
	}

	return data;
}

LUA_FUNCTION_STATIC( tostring )
{
	lua_pushfstring( LUA->GetState( ), "%s: %p", metaname, Get( LUA, 1 ) );
	return 1;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( LUA, 1 ) == Get( LUA, 2 ) );
	return 1;
}

LUA_FUNCTION_STATIC( index )
{
	LUA->GetMetaTable( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	if( !LUA->IsType( -1, GarrysMod::Lua::Type::NIL ) )
		return 1;

	LUA->Pop( 2 );

	lua_getfenv( LUA->GetState( ), 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	lua_getfenv( LUA->GetState( ), 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( Close )
{
	CheckType( LUA, 1 );
	Container *container = LUA->GetUserType<Container>( 1, metatype );

	Base *file = container->file;
	if( file == nullptr )
		return 0;

	delete file;
	container->file = nullptr;
	return 0;
}

LUA_FUNCTION_STATIC( IsValid )
{
	CheckType( LUA, 1 );
	LUA->PushBool( LUA->GetUserType<Container>( 1, metatype )->file != nullptr );
	return 1;
}

LUA_FUNCTION_STATIC( EndOfFile )
{
	LUA->PushBool( Get( LUA, 1 )->EndOfFile( ) );
	return 1;
}

LUA_FUNCTION_STATIC( OK )
{
	LUA->PushBool( Get( LUA, 1 )->Good( ) );
	return 1;
}

LUA_FUNCTION_STATIC( Size )
{
	LUA->PushNumber( static_cast<double>( Get( LUA, 1 )->Size( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Tell )
{
	LUA->PushNumber( static_cast<double>( Get( LUA, 1 )->Tell( ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Seek )
{
	Base *file = Get( LUA, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	SeekDirection seektype = SeekBeg;
	if( LUA->IsType( 3, GarrysMod::Lua::Type::NUMBER ) )
	{
		uint32_t num = static_cast<uint32_t>( LUA->GetNumber( 3 ) );
		if( num >= static_cast<uint32_t>( SeekBeg ) && num <= static_cast<uint32_t>( SeekEnd ) )
			seektype = static_cast<SeekDirection>( num );
	}

	LUA->PushBool( file->Seek( static_cast<int64_t>( LUA->GetNumber( 2 ) ), seektype ) );
	return 0;
}

LUA_FUNCTION_STATIC( Flush )
{
	LUA->PushBool( Get( LUA, 1 )->Flush( ) );
	return 1;
}

LUA_FUNCTION_STATIC( InvertBytes )
{
	CheckType( LUA, 1 );
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );
	LUA->GetUserType<Container>( 1, metatype )->invert = LUA->GetBool( 2 );
	return 0;
}

LUA_FUNCTION_STATIC( Read )
{
	Base *file = Get( LUA, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double len = LUA->GetNumber( 2 );
	if( len < 1.0 || len > 4294967295.0 )
		LUA->ArgError( 2, "size out of bounds, must fit in a 32 bits unsigned integer and be bigger than 0" );

	std::vector<char> buffer( static_cast<size_t>( len ) );
	size_t read = file->Read( buffer.data( ), buffer.size( ) );
	if( read > 0 )
	{
		LUA->PushString( buffer.data( ), read );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadString )
{
	Base *file = Get( LUA, 1 );

	int64_t pos = file->Tell( );

	char c = '\0';
	std::string buffer;
	while( !file->EndOfFile( ) && file->Read( &c, 1 ) == 1 )
	{
		if( c == '\0' )
		{
			LUA->PushString( buffer.c_str( ) );
			return 1;
		}

		buffer += c;
	}

	file->Seek( pos, SeekBeg );
	return 0;
}

LUA_FUNCTION_STATIC( ReadInt )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	size_t bits = static_cast<size_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( file->Tell( ) + sizeof( int8_t ) >= file->Size( ) )
				return 0;

			int8_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( file->Tell( ) + sizeof( int16_t ) >= file->Size( ) )
				return 0;

			int16_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( file->Tell( ) + sizeof( int32_t ) >= file->Size( ) )
				return 0;

			int32_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( file->Tell( ) + sizeof( int64_t ) >= file->Size( ) )
				return 0;

			int64_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( static_cast<double>( InvertBytes( num, invert ) ) );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadUInt )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	size_t bits = static_cast<size_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( file->Tell( ) + sizeof( uint8_t ) >= file->Size( ) )
				return 0;

			uint8_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( file->Tell( ) + sizeof( uint16_t ) >= file->Size( ) )
				return 0;

			uint16_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( file->Tell( ) + sizeof( uint32_t ) >= file->Size( ) )
				return 0;

			uint32_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( file->Tell( ) + sizeof( uint64_t ) >= file->Size( ) )
				return 0;

			uint64_t num = 0;
			file->Read( &num, sizeof( num ) );
			LUA->PushNumber( static_cast<double>( InvertBytes( num, invert ) ) );
			break;
		}

		default:
			LUA->ArgError( 2, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( ReadFloat )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );

	if( file->Tell( ) + sizeof( float ) >= file->Size( ) )
		return 0;

	float num = 0.0f;
	file->Read( &num, sizeof( num ) );
	LUA->PushNumber( InvertBytes( num, invert ) );
	return 1;
}

LUA_FUNCTION_STATIC( ReadDouble )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );

	if( file->Tell( ) + sizeof( double ) >= file->Size( ) )
		return 0;

	double num = 0.0;
	file->Read( &num, sizeof( num ) );
	LUA->PushNumber( InvertBytes( num, invert ) );
	return 1;
}

LUA_FUNCTION_STATIC( Write )
{
	Base *file = Get( LUA, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	LUA->PushNumber( len != 0 ? file->Write( str, len ) : 0.0 );
	return 1;
}

LUA_FUNCTION_STATIC( WriteString )
{
	Base *file = Get( LUA, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	char z = '\0';
	if( len != 0 )
		LUA->PushNumber( file->Write( str, len ) + file->Write( &z, 1 ) );
	else
		LUA->PushNumber( 0.0 );

	return 1;
}

LUA_FUNCTION_STATIC( WriteInt )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	size_t bits = static_cast<size_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			int8_t num = static_cast<int8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 16:
		{
			int16_t num = InvertBytes( static_cast<int16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 32:
		{
			int32_t num = InvertBytes( static_cast<int32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 64:
		{
			int64_t num = InvertBytes( static_cast<int64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteUInt )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	size_t bits = static_cast<size_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			uint8_t num = static_cast<uint8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 16:
		{
			uint16_t num = InvertBytes( static_cast<uint16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 32:
		{
			uint32_t num = InvertBytes( static_cast<uint32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		case 64:
		{
			uint64_t num = InvertBytes( static_cast<uint64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( file->Write( &num, sizeof( num ) ) == bits );
			break;
		}

		default:
			LUA->ArgError( 3, "number of bits requested is not supported, must be 8, 16, 32 or 64" );
	}

	return 1;
}

LUA_FUNCTION_STATIC( WriteFloat )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	float num = InvertBytes( static_cast<float>( LUA->GetNumber( 2 ) ), invert );
	LUA->PushBool( file->Write( &num, sizeof( num ) ) == sizeof( num ) );
	return 1;
}

LUA_FUNCTION_STATIC( WriteDouble )
{
	bool invert = false;
	Base *file = Get( LUA, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double num = InvertBytes( LUA->GetNumber( 2 ), invert );
	LUA->PushBool( file->Write( &num, sizeof( num ) ) == sizeof( num ) );
	return 1;
}

void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	metatype = LUA->CreateMetaTable( metaname );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

	LUA->PushCFunction( index );
	LUA->SetField( -2, "__index" );

	LUA->PushCFunction( newindex );
	LUA->SetField( -2, "__newindex" );

	LUA->PushCFunction( Close );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( Close );
	LUA->SetField( -2, "Close" );

	LUA->PushCFunction( IsValid );
	LUA->SetField( -2, "IsValid" );

	LUA->PushCFunction( EndOfFile );
	LUA->SetField( -2, "EOF" );

	LUA->PushCFunction( OK );
	LUA->SetField( -2, "OK" );

	LUA->PushCFunction( Size );
	LUA->SetField( -2, "Size" );

	LUA->PushCFunction( Tell );
	LUA->SetField( -2, "Tell" );

	LUA->PushCFunction( Seek );
	LUA->SetField( -2, "Seek" );

	LUA->PushCFunction( Flush );
	LUA->SetField( -2, "Flush" );

	LUA->PushCFunction( InvertBytes );
	LUA->SetField( -2, "InvertBytes" );

	LUA->PushCFunction( Read );
	LUA->SetField( -2, "Read" );

	LUA->PushCFunction( ReadString );
	LUA->SetField( -2, "ReadString" );

	LUA->PushCFunction( ReadInt );
	LUA->SetField( -2, "ReadInt" );

	LUA->PushCFunction( ReadUInt );
	LUA->SetField( -2, "ReadUInt" );

	LUA->PushCFunction( ReadFloat );
	LUA->SetField( -2, "ReadFloat" );

	LUA->PushCFunction( ReadDouble );
	LUA->SetField( -2, "ReadDouble" );

	LUA->PushCFunction( Write );
	LUA->SetField( -2, "Write" );

	LUA->PushCFunction( WriteString );
	LUA->SetField( -2, "WriteString" );

	LUA->PushCFunction( WriteInt );
	LUA->SetField( -2, "WriteInt" );

	LUA->PushCFunction( WriteUInt );
	LUA->SetField( -2, "WriteUInt" );

	LUA->PushCFunction( WriteFloat );
	LUA->SetField( -2, "WriteFloat" );

	LUA->PushCFunction( WriteDouble );
	LUA->SetField( -2, "WriteDouble" );

	LUA->Pop( 1 );
}

void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, metaname );
}

}
