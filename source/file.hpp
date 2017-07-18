#pragma once

namespace GarrysMod
{
	namespace Lua
	{
		class ILuaBase;
	}
}

namespace file
{

class Base;
	
void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );
void Create( GarrysMod::Lua::ILuaBase *LUA, Base *file );

}
