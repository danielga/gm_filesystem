#pragma once

struct lua_State;

namespace file
{

class Base;
	
void Initialize( lua_State *state );
void Deinitialize( lua_State *state );
void Create( lua_State *state, Base *file );

}
