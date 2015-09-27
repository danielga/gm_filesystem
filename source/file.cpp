#include <GarrysMod/Lua/Interface.h>
#include <main.hpp>
#include <lua.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <filesystem.h>

namespace filehandle
{

static const char *metaname = "FileHandle_t";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT;
static const char *invalid_error = "invalid FileHandle_t";

struct UserData
{
	FileHandle_t file;
	uint8_t type;
	bool invert;
};

inline void CheckType( lua_State *state, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( state, index, metaname );
}

static FileHandle_t Get( lua_State *state, int32_t index, bool *invert = nullptr )
{
	CheckType( state, index );
	UserData *udata = static_cast<UserData *>( LUA->GetUserdata( index ) );
	FileHandle_t file = udata->file;
	if( file == nullptr )
		LUA->ArgError( index, invalid_error );

	if( invert != nullptr )
		*invert = udata->invert;

	return file;
}

void Create( lua_State *state, FileHandle_t file )
{
	UserData *udata = static_cast<UserData *>( LUA->NewUserdata( sizeof( UserData ) ) );
	udata->file = file;
	udata->type = metatype;
	udata->invert = false;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( state, -2 );
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
	lua_pushfstring( state, "%s: %p", metaname, Get( state, 1 ) );
	return 1;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( state, 1 ) == Get( state, 2 ) );
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

	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( Close )
{
	CheckType( state, 1 );
	UserData *udata = static_cast<UserData *>( LUA->GetUserdata( 1 ) );

	FileHandle_t file = udata->file;
	if( file == nullptr )
		return 0;

	global::filesystem->Close( file );
	udata->file = nullptr;
	return 0;
}

LUA_FUNCTION_STATIC( IsValid )
{
	CheckType( state, 1 );
	LUA->PushBool( static_cast<UserData *>( LUA->GetUserdata( 1 ) )->file != nullptr );
	return 1;
}

LUA_FUNCTION_STATIC( EndOfFile )
{
	LUA->PushBool( global::filesystem->EndOfFile( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( OK )
{
	LUA->PushBool( global::filesystem->IsOk( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Size )
{
	LUA->PushNumber( global::filesystem->Size( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Tell )
{
	LUA->PushNumber( global::filesystem->Tell( Get( state, 1 ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( Seek )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double pos = LUA->GetNumber( 2 );
	if( pos < -2147483648.0 || pos > 2147483647.0 )
		LUA->ArgError( 2, "position out of bounds, must fit in a 32 bits signed integer" );

	FileSystemSeek_t seektype = FILESYSTEM_SEEK_HEAD;
	if( LUA->IsType( 3, GarrysMod::Lua::Type::NUMBER ) )
	{
		uint32_t num = static_cast<uint32_t>( LUA->GetNumber( 3 ) );
		if( num >= FILESYSTEM_SEEK_HEAD && num <= FILESYSTEM_SEEK_TAIL )
			seektype = static_cast<FileSystemSeek_t>( num );
	}

	global::filesystem->Seek( file, static_cast<int32_t>( pos ), seektype );
	return 0;
}

LUA_FUNCTION_STATIC( Flush )
{
	global::filesystem->Flush( Get( state, 1 ) );
	return 0;
}

LUA_FUNCTION_STATIC( InvertBytes )
{
	CheckType( state, 1 );
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );
	static_cast<UserData *>( LUA->GetUserdata( 1 ) )->invert = LUA->GetBool( 2 );
	return 0;
}

LUA_FUNCTION_STATIC( Read )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double len = LUA->GetNumber( 2 );
	if( len < 1.0 || len > 2147483647.0 )
		LUA->ArgError( 2, "size out of bounds, must fit in a 32 bits signed integer and be bigger than 0" );

	std::vector<char> buffer( static_cast<int32_t>( len ) );

	int32_t read = global::filesystem->Read( buffer.data( ), buffer.size( ), file );
	if( read > 0 )
	{
		LUA->PushString( buffer.data( ), read );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC( ReadString )
{
	FileHandle_t file = Get( state, 1 );

	uint32_t pos = global::filesystem->Tell( file );

	char c = '\0';
	std::string buffer;
	while( !global::filesystem->EndOfFile( file ) && global::filesystem->Read( &c, 1, file ) == 1 )
	{
		if( c == '\0' )
		{
			LUA->PushString( buffer.c_str( ) );
			return 1;
		}

		buffer += c;
	}

	global::filesystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
	return 0;
}

LUA_FUNCTION_STATIC( ReadInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( global::filesystem->Tell( file ) + sizeof( int8_t ) >= global::filesystem->Size( file ) )
				return 0;

			int8_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( global::filesystem->Tell( file ) + sizeof( int16_t ) >= global::filesystem->Size( file ) )
				return 0;

			int16_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( global::filesystem->Tell( file ) + sizeof( int32_t ) >= global::filesystem->Size( file ) )
				return 0;

			int32_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( global::filesystem->Tell( file ) + sizeof( int64_t ) >= global::filesystem->Size( file ) )
				return 0;

			int64_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
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
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 2 ) );
	switch( bits )
	{
		case 8:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint8_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint8_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( num );
			break;
		}

		case 16:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint16_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint16_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 32:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint32_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint32_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
			break;
		}

		case 64:
		{
			if( global::filesystem->Tell( file ) + sizeof( uint64_t ) >= global::filesystem->Size( file ) )
				return 0;

			uint64_t num = 0;
			global::filesystem->Read( &num, sizeof( num ), file );
			LUA->PushNumber( InvertBytes( num, invert ) );
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
	FileHandle_t file = Get( state, 1, &invert );

	if( global::filesystem->Tell( file ) + sizeof( float ) >= global::filesystem->Size( file ) )
		return 0;

	float num = 0.0f;
	global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( InvertBytes( num, invert ) );
	return 1;
}

LUA_FUNCTION_STATIC( ReadDouble )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );

	if( global::filesystem->Tell( file ) + sizeof( double ) >= global::filesystem->Size( file ) )
		return 0;

	double num = 0.0;
	global::filesystem->Read( &num, sizeof( num ), file );
	LUA->PushNumber( InvertBytes( num, invert ) );
	return 1;
}

LUA_FUNCTION_STATIC( Write )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	if( len > 2147483647 )
		len = 2147483647;

	LUA->PushNumber( len != 0 ? global::filesystem->Write( str, len, file ) : 0.0 );
	return 1;
}

LUA_FUNCTION_STATIC( WriteString )
{
	FileHandle_t file = Get( state, 1 );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	size_t len = 0;
	const char *str = LUA->GetString( 2, &len );

	if( len > 2147483647 )
		len = 2147483647;

	char z = '\0';
	if( len != 0 )
		LUA->PushNumber(
			global::filesystem->Write( str, len, file ) + global::filesystem->Write( &z, 1, file )
		);
	else
		LUA->PushNumber( 0.0 );

	return 1;
}

LUA_FUNCTION_STATIC( WriteInt )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			int8_t num = static_cast<int8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			int16_t num = InvertBytes( static_cast<int16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			int32_t num = InvertBytes( static_cast<int32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			int64_t num = InvertBytes( static_cast<int64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
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
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );
	LUA->CheckType( 3, GarrysMod::Lua::Type::NUMBER );

	int32_t bits = static_cast<int32_t>( LUA->GetNumber( 3 ) );
	switch( bits )
	{
		case 8:
		{
			uint8_t num = static_cast<uint8_t>( LUA->GetNumber( 2 ) );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 16:
		{
			uint16_t num = InvertBytes( static_cast<uint16_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 32:
		{
			uint32_t num = InvertBytes( static_cast<uint32_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
			break;
		}

		case 64:
		{
			uint64_t num = InvertBytes( static_cast<uint64_t>( LUA->GetNumber( 2 ) ), invert );
			LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == bits );
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
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	float num = InvertBytes( static_cast<float>( LUA->GetNumber( 2 ) ), invert );
	LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == sizeof( num ) );
	return 1;
}

LUA_FUNCTION_STATIC( WriteDouble )
{
	bool invert = false;
	FileHandle_t file = Get( state, 1, &invert );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	double num = InvertBytes( LUA->GetNumber( 2 ), invert );
	LUA->PushBool( global::filesystem->Write( &num, sizeof( num ), file ) == sizeof( num ) );
	return 1;
}

void Initialize( lua_State *state )
{
	LUA->CreateMetaTableType( metaname, metatype );

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

void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, metaname );
}

}
