#include "gbk.h"

#define LUA_LIB
#include "ldbcs.h"

LUALIB_API int luaopen_gbk(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Ldbcs_##name }
        ENTRY(len),
        ENTRY(byte),
        ENTRY(char),
        ENTRY(toutf8),
        ENTRY(fromutf8),
#undef  ENTRY
        { NULL, NULL }
    };
#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, libs);
#else
    lua_newtable(L);
    luaL_register(L, NULL, libs);
#endif
    return 1;
}

/* cc: flags+='-s -mdll -O2 -DLUA_BUILD_AS_DLL'
 * cc: libs+='-llua53.dll' output='gbk.dll' run='lua.exe test.lua' */

