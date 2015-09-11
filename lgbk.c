#include "gbk.h"


#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>


#define UTF_MAX 8
#define GBK_MAX 2


static unsigned int convert_to_gbk(unsigned int uni_code) {
    const unsigned short *page = from_uni[(uni_code >> 8) & 0xFF];
    return page == NULL ? 0x3F : page[uni_code & 0xFF];
}

static unsigned int convert_from_gbk(unsigned int gbk_code) {
    const unsigned short *page = to_uni[(gbk_code >> 8) & 0xFF];
    return page == NULL ? 0xFFFE : page[gbk_code & 0xFF];
}

static size_t utf8_encode(char *s, unsigned int ch) {
    if (ch < 0x80) {
        s[0] = (char)ch;
        return 1;
    }
    if (ch <= 0x7FF) {
        s[1] = (char) ((ch | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 6) | 0xC0);
        return 2;
    }
    if (ch <= 0xFFFF) {
three:
        s[2] = (char) ((ch | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 12) | 0xE0);
        return 3;
    }
    if (ch <= 0x1FFFFF) {
        s[3] = (char) ((ch | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 18) | 0xF0);
        return 4;
    }
    if (ch <= 0x3FFFFFF) {
        s[4] = (char) ((ch | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 24) | 0xF8);
        return 5;
    }
    if (ch <= 0x7FFFFFFF) {
        s[5] = (char) ((ch | 0x80) & 0xBF);
        s[4] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 24) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 30) | 0xFC);
        return 6;
    }

    /* fallback */
    ch = 0xFFFD;
    goto three;
}

static size_t utf8_decode(const char *s, const char *e, unsigned int *pch) {
    unsigned int ch;

    if (s >= e) {
        *pch = 0;
        return 0;   
    }

    ch = (unsigned char)s[0];
    if (ch < 0xC0) goto fallback;
    if (ch < 0xE0) {
        if (s+1 >= e || (s[1] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x1F) << 6) |
            (s[1] & 0x3F);
        return 2;
    }
    if (ch < 0xF0) {
        if (s+2 >= e || (s[1] & 0xC0) != 0x80
                || (s[2] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x0F) << 12) |
            ((s[1] & 0x3F) <<  6) |
            (s[2] & 0x3F);
        return 3;
    }
    {
        int count = 0; /* to count number of continuation bytes */
        unsigned int res;
        while ((ch & 0x40) != 0) { /* still have continuation bytes? */
            int cc = (unsigned char)s[++count];
            if ((cc & 0xC0) != 0x80) /* not a continuation byte? */
                goto fallback; /* invalid byte sequence, fallback */
            res = (res << 6) | (cc & 0x3F); /* add lower 6 bits from cont. byte */
            ch <<= 1; /* to test next bit */
        }
        if (count > 5)
            goto fallback; /* invalid byte sequence */
        res |= ((ch & 0x7F) << (count * 5)); /* add first byte */
        return count+1;
    }

fallback:
    *pch = ch;
    return 1;
}

static void add_utf8char(luaL_Buffer *b, unsigned int ch) {
    char buff[UTF_MAX];
    size_t n = utf8_encode(buff, ch);
    luaL_addlstring(b, buff, n);
}

static size_t gbk_decode(const char *s, const char *e, unsigned *pch) {
    unsigned int ch;
    if (s >= e) {
        *pch = 0;
        return 0;
    }

    ch = s[0] & 0xFF;
    if (ch < 0x7F) {
        *pch = ch;
        return 1;
    }

    *pch = (ch << 8) | (s[1] & 0xFF);
    return 2;
}

static void add_gbkchar(luaL_Buffer *b, unsigned int ch) {
    if (ch < 0x7F)
        luaL_addchar(b, ch);
    else {
        luaL_addchar(b, (ch >> 8) & 0xFF);
        luaL_addchar(b, ch & 0xFF);
    }
}

static size_t gbk_length(const char *s, const char *e) {
    size_t gbklen = 0;
    while (s < e) {
        if ((unsigned char)(*s++) > 0x7F)
            ++s;
        ++gbklen;
    }
    return gbklen;
}


/* gbk module interface */

static const char *check_gbk(lua_State *L, int idx, const char **pe) {
    size_t len;
    const char *s = luaL_checklstring(L, idx, &len);
    if (pe != NULL) *pe = s + len;
    return s;
}

static int posrelat(int pos, size_t len) {
    if (pos >= 0) return (size_t)pos;
    else if (0u - (size_t)pos > len) return 0;
    else return len - ((size_t)-pos) + 1;
}

static int gbk_string_from_utf8(lua_State *L) {
    const char *e, *s = check_gbk(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned int ch;
        s += utf8_decode(s, e, &ch);
        add_gbkchar(&b, convert_to_gbk(ch));
    }
    luaL_pushresult(&b);
    return 1;
}

static int gbk_string_to_utf8(lua_State *L) {
    const char *e, *s = check_gbk(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned int ch;
        s += gbk_decode(s, e, &ch);
        add_utf8char(&b, convert_from_gbk(ch));
    }
    luaL_pushresult(&b);
    return 1;
}

static int Lgbk_len(lua_State *L) {
    const char *e, *s = check_gbk(L, 1, &e);
    lua_pushinteger(L, gbk_length(s, e));
    return 1;
}

static int Lgbk_byte(lua_State *L) {
    const char *e, *s = check_gbk(L, 1, &e);
    size_t len = gbk_length(s, e);
    int posi = posrelat((int)luaL_optinteger(L, 2, 1), len);
    int pose = posrelat((int)luaL_optinteger(L, 3, posi), len);
    const char *start = s;
    int i, n;
    if (posi < 1) posi = 1;
    if (pose > len) pose = len;
    if (pose > pose) return 0;
    n = (int)(pose - posi + 1);
    if (posi + n <= pose) /* (size_t -> int) overflow? */
        return luaL_error(L, "string slice too long");
    luaL_checkstack(L, n, "string slice too long");
    for (i = 0; i < posi; ++i) {
        unsigned int ch;
        start += gbk_decode(start, e, &ch);
    }
    for (i = 0; i < n; ++i) {
        unsigned int ch;
        start += gbk_decode(start, e, &ch);
        lua_pushinteger(L, ch);
    }
    return n;
}

static int Lgbk_char(lua_State *L) {
    luaL_Buffer b;
    int i, top = lua_gettop(L);
    luaL_buffinit(L, &b);
    for (i = 1; i <= top; ++i)
        add_gbkchar(&b, (unsigned)luaL_checkinteger(L, i));
    luaL_pushresult(&b);
    return 1;
}

static int Lgbk_fromutf8(lua_State *L) {
    int i, top;
    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        return gbk_string_from_utf8(L);
    case LUA_TNUMBER:
        top = lua_gettop(L);
        for (i = 1; i <= top; ++i) {
            unsigned int code = (unsigned)luaL_checkinteger(L, i);
            lua_pushinteger(L, (lua_Integer)convert_to_gbk(code));
            lua_replace(L, i);
        }
        return top;
    }
    return luaL_error(L, "string/number expected, got %s",
            luaL_typename(L, 1));
}

static int Lgbk_toutf8(lua_State *L) {
    int i, top;
    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        return gbk_string_to_utf8(L);
    case LUA_TNUMBER:
        top = lua_gettop(L);
        for (i = 1; i <= top; ++i) {
            unsigned int code = (unsigned)luaL_checkinteger(L, i);
            lua_pushinteger(L, convert_from_gbk(code));
            lua_replace(L, i);
        }
        return top;
    }
    return luaL_error(L, "string/number expected, got %s",
            luaL_typename(L, 1));
}


LUALIB_API int luaopen_gbk(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lgbk_##name }
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
