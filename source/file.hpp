#pragma once

struct lua_State;
typedef void *FileHandle_t;

namespace filehandle
{
	
void Initialize( lua_State *state );
void Deinitialize( lua_State *state );
void Create( lua_State *state, FileHandle_t file );

}
