#pragma once

namespace GarrysMod
{
	namespace Lua
	{
		class ILuaBase;
	}
}

namespace filesystem
{

void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );

}
