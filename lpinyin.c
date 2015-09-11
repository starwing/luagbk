#include "pinyin.h"


#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>

#include <string.h>


#define UTF_MAX     8

/* add_pinyin flags */
#define WITH_UTF8      0x01
#define WITH_TONE      0x02
#define WITH_PREFIX    0x04
#define WITH_POSTFIX   0x08


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

static const char *utf8_next(const char *s, const char *e) {
    unsigned int ch;
    return s + utf8_decode(s, e, &ch);
}

static const char *utf8_prev(const char *s, const char *e) {
    const char *look = e - 1;

    while (s <= look) {
        unsigned int ch = (unsigned char)*look;
        if (ch < 0x80 || ch >= 0xC0)
            return look;
        --look;
    }

    return s;
}

static const char *utf8_index(const char *s, const char *e, int idx) {
    if (idx >= 0) {
        while (s < e && --idx > 0)
            s = utf8_next(s, e);
        return s;
    }
    else {
        while (s < e && idx++ < 0)
            e = utf8_prev(s, e);
        return e;
    }
}

static void add_utf8char(luaL_Buffer *b, unsigned int ch) {
    char buff[UTF_MAX];
    size_t n = utf8_encode(buff, ch);
    luaL_addlstring(b, buff, n);
}

static void add_utf8_tone(luaL_Buffer *b, int rhyme, int tone) {
    static unsigned int utf8_rhyms[][5] = {
        { 0x0061, 0x0101, 0x00E1, 0x01CE, 0x00E0 }, /* a */
        { 0x0069, 0x012B, 0x00ED, 0x01D0, 0x00EC }, /* i */
        { 0x0075, 0x016B, 0x00FA, 0x01D4, 0x00F9 }, /* u */
        { 0x0065, 0x0113, 0x00E9, 0x011B, 0x00E8 }, /* e */
        { 0x006F, 0x014D, 0x00F3, 0x01D2, 0x00F2 }, /* o */
        { 0x00FC, 0x01D6, 0x01D8, 0x01DA, 0x01DC }, /* v */
        { 0x006D, 0x0000, 0x1E3F,                }, /* m */
        { 0x006E, 0x0000, 0x0144, 0x0148, 0x01F9 }, /* n */
        { 0x0067, 0x0000, 0x01F5, 0x01E7,        }, /* g */
    };
    static unsigned int combine[4] = {
        0x0304, 0x0301, 0x030C, 0x0300
    };
    int ch, idx;
    switch (rhyme) {
    case 'a': idx = 0; break;
    case 'i': idx = 1; break;
    case 'u': idx = 2; break;
    case 'e': idx = 3; break;
    case 'o': idx = 4; break;
    case 'v': idx = 5; break;
    case 'm': idx = 6; break;
    case 'n': idx = 7; break;
    case 'g': idx = 8; break;
    default:  idx = -1; break;
    }
    if (idx < 0) {
        luaL_addchar(b, rhyme);
        return;
    }

    ch = utf8_rhyms[idx][tone];
    if (ch != 0)
        add_utf8char(b, ch);
    else {
        luaL_addchar(b, rhyme);
        if (tone != 0) 
            add_utf8char(b, combine[tone-1]);
    }
}

static void add_utf8_rhyme(luaL_Buffer *b, const char *rhyme, int tone) {
    if ((rhyme[0] != 'i' && rhyme[0] != 'u' && rhyme[0] != 'v') ||
            (rhyme[1] != 'a' && rhyme[1] != 'e' && rhyme[1] != 'o')) {
        add_utf8_tone(b, rhyme[0], tone);
        luaL_addstring(b, &rhyme[1]);
    }
    else {
        luaL_addchar(b, rhyme[0]);
        add_utf8_tone(b, rhyme[1], tone);
        luaL_addstring(b, &rhyme[2]);
    }
}

static void add_pinyin(luaL_Buffer *b, const PinyinEntry *entry, int flags) {
    const char *rhyme;
    if ((flags & WITH_PREFIX) != 0)
        luaL_addchar(b, ' ');
    luaL_addstring(b, py_syllables[(size_t)entry->syllable]);
    rhyme = py_rhymes[(size_t)entry->rhyme];
    if ((flags & WITH_UTF8) != 0)
        add_utf8_rhyme(b, rhyme, entry->tone);
    else
        luaL_addstring(b, rhyme);
    if ((flags & WITH_TONE) != 0)
        luaL_addchar(b, '0' + entry->tone);
    if ((flags & WITH_POSTFIX) != 0)
        luaL_addchar(b, ' ');
}

static const PinyinEntry *get_entry(unsigned int uni_code) {
    const PinyinEntry *page = pytable[(uni_code >> 8) & 0xFF];
    return page == NULL ? NULL : &page[uni_code & 0xFF];
}

static const PinyinEntry *get_polyphone(unsigned int uni_code) {
    size_t b = 0, t = POLYPHONE_COUNT;
    while (b < t) {
        size_t mid = (b + t)>>1;
        const PinyinPolyphone *cur = &polyphone[mid];
        /*printf("%X: %X (%d, %d)\n", uni_code, (unsigned)cur->cp, b, t);*/
        if (uni_code > cur->cp)
            b = mid+1;
        else if (uni_code < cur->cp)
            t = mid;
        else
            return &polyphone_data[cur->idx];
    }
    return NULL;
}


/* parse pinyin */

static int starts_with(const char *s, const char *prefix, const char **ends) {
    int res;
    while (*prefix != '\0' && *s == *prefix)
        ++s, ++prefix;
    res = *prefix == '\0' ? 0 : *s - *prefix;
    if (ends)
        *ends = s;
    return res;
}

static const char *skip_whitespace(const char *s) {
    while (*s == '\t' || *s == '\n' || *s == '\r'
            || *s == ' ' || *s == '\'')
        ++s;
    return s;
}

static const char *pinyin_syllable(const char *pinyin, PinyinEntry *e) {
    int may_h = 0;
    switch (*pinyin) {
    case 'b': e->syllable = 1;  break;
    case 'c': e->syllable = 2; may_h = 1;  break;
    case 'd': e->syllable = 4;  break;
    case 'f': e->syllable = 5;  break;
    case 'g': e->syllable = 6;  break;
    case 'h': e->syllable = 7;  break;
    case 'j': e->syllable = 8;  break;
    case 'k': e->syllable = 9;  break;
    case 'l': e->syllable = 10; break;
    case 'm': e->syllable = 11; break;
    case 'n': e->syllable = 12; break;
    case 'p': e->syllable = 13; break;
    case 'q': e->syllable = 14; break;
    case 'r': e->syllable = 15; break;
    case 's': e->syllable = 16; may_h = 1; break;
    case 't': e->syllable = 18; break;
    case 'w': e->syllable = 19; break;
    case 'x': e->syllable = 20; break;
    case 'y': e->syllable = 21; break;
    case 'z': e->syllable = 22; may_h = 1; break;
    default:  e->syllable = 0;  return pinyin;
    }
    ++pinyin;
    if (may_h && *pinyin == 'h')
        ++e->syllable, ++pinyin;
    return pinyin;
}

static const char *pinyin_rhyme(const char *pinyin, PinyinEntry *e) {
    const char *ends;
    int res, mid, b = 0, t = RHYME_COUNT;
    while (b < t) {
        mid = (b + t - 1) >> 1;
        res = starts_with(pinyin, py_rhymes[mid], &ends);
        printf("%d:%d:%d: %s - %s\n", b, mid, t, pinyin, py_rhymes[mid]);
        if (b == mid)
            break;
        if (res >= 0)
            b = mid;
        else
            t = mid;
    }
    if (res == 0) {
        e->rhyme = mid;
        return ends;
    }
    return pinyin;
}

static const char *pinyin_tone(const char *pinyin, PinyinEntry *e) {
    if (*pinyin >= '1' && *pinyin <= '4') {
        e->tone = *pinyin - '0';
        return pinyin+1;
    }
    return pinyin;
}

static const char *parse_pinyin(const char *pinyin, PinyinEntry *e) {
    const char *syl, *rhy, *tone;
    syl = skip_whitespace(pinyin);
    e->syllable = 0;
    e->rhyme = 0;
    e->tone = 0;
    switch (*pinyin) {
    case 'a':
    case 'e':
    case 'o':
no_syllable:
        tone = pinyin_rhyme(syl, e);
        return pinyin_tone(tone, e);
    }
    if ((rhy = pinyin_syllable(syl, e)) == syl)
        return pinyin;
    if ((tone = pinyin_rhyme(rhy, e)) == rhy) {
        if (*syl == 'm' || *syl == 'n' || *syl == 'r')
            goto no_syllable;
        return pinyin;
    }
    return pinyin_tone(tone, e);
}

static const PinyinIndex *get_pyindex(PinyinEntry entry) {
    size_t b = 0, t = PYINDEX_COUNT;
    unsigned key = (entry.syllable<<16)|(entry.rhyme<<8)|(entry.tone);
    while (b < t) {
        size_t mid = (b + t-1)>>1;
        const PinyinIndex *cur = &pyindex[mid];
        unsigned curkey = (cur->entry.syllable<<16)|(cur->entry.rhyme<<8)|(cur->entry.tone);
        printf("%d:%d:%d: (%08X) - %08X\n", b, mid, t, curkey, key);
        if (key > curkey)
            b = mid+1;
        else if (key < curkey)
            t = mid;
        else
            return cur;
    }
    return NULL;
}


/* lua routines */

static const char *check_utf8(lua_State *L, int idx, const char **end) {
    size_t len;
    const char *s = luaL_checklstring(L, idx, &len);
    if (end) *end = s+len;
    return s;
}

static int u_posrange(const char **ps, const char **pe,
    lua_Integer posi, lua_Integer posj) {
  const char *s = *ps, *e = *pe;
  *ps = utf8_index(s, e, posi);
  if (posj >= 0) {
    while (s < e && posj-- > 0)
      s = utf8_next(s, e);
    *pe = s;
  }
  else {
    while (s < e && ++posj < 0)
      e = utf8_prev(s, e);
    *pe = e;
  }
  return *ps < *pe;
}

static int posrelat_raw(lua_Integer idx, size_t len) {
    if (idx > 0) --idx;
    else if (idx < 0) idx += len;
    if (idx < 0) idx = 0;
    else if (idx >= len) idx = len;
    return (int)idx;
}

static int Lpinyin(lua_State *L) {
    luaL_Buffer b;
    const char *e, *s = check_utf8(L, 1, &e);
    const char *opt = luaL_optstring(L, 2, NULL);
    int res = 1;
    int baseflags = 0;
    if (!u_posrange(&s, &e,
                luaL_optinteger(L, 3, 1), luaL_optinteger(L, 4, -1)))
        return 0;
    if (opt && *opt == 'u')
        baseflags |= WITH_UTF8;
    if (opt && *opt == 't')
        baseflags |= WITH_TONE;
    luaL_buffinit(L, &b);
    while (s < e) {
        const PinyinEntry *entry;
        int flags = baseflags;
        unsigned int ch;
        s += utf8_decode(s, e, &ch);
        if (!res)
            flags |= WITH_PREFIX;
        if (s < e)
            flags |= WITH_POSTFIX;
        if ((entry = get_entry(ch)) != NULL)
            add_pinyin(&b, entry, flags);
        else
            add_utf8char(&b, ch);
    }
    luaL_pushresult(&b);
    return 1;
}

static int get_codepoint(lua_State *L, int sidx, int idx) {
    const char *e, *s = check_utf8(L, sidx, &e);
    if (u_posrange(&s, &e, luaL_optinteger(L, idx, 1), luaL_optinteger(L, idx+1, 1))) {
        unsigned int ch;
        if (!utf8_decode(s, e, &ch))
            return 0;
        return ch;
    }
    return 0;
}

static int get_info(lua_State *L, const PinyinEntry *entry, const char *opt) {
    luaL_Buffer b;
    if (opt == NULL) {
        luaL_buffinit(L, &b);
        add_pinyin(&b, entry, 0);
        luaL_pushresult(&b);
        return 1;
    }
    switch (*opt) {
    case 's':
        lua_pushstring(L, py_syllables[(size_t)entry->syllable]);
        return 1;
    case 'r':
        lua_pushstring(L, py_rhymes[(size_t)entry->rhyme]);
        return 1;
    case 't':
        lua_pushinteger(L, entry->tone);
        return 1;
    case 'p':
        lua_pushinteger(L, entry->polyphone_count);
        return 1;
    case 'u':
        luaL_buffinit(L, &b);
        if (opt[1] != 'r' && opt[1] != 't')
            add_pinyin(&b, entry, WITH_UTF8);
        else if (opt[1] == 'r' || strcmp(opt, "utf8rhyme") == 0)
            add_utf8_rhyme(&b, py_rhymes[(size_t)entry->rhyme], entry->tone);
        return 1;
    }
    return luaL_argerror(L, 2, "invalid option");
}

static int Linfo(lua_State *L) {
    const PinyinEntry *entry;
    unsigned int ch = get_codepoint(L, 1, 3);
    const char *opt = luaL_optstring(L, 2, NULL);
    if ((entry = get_entry(ch)) == NULL)
        return 0;
    return get_info(L, entry, opt);
}

static int Lpolyphone(lua_State *L) {
    const PinyinEntry *entry;
    unsigned int ch, idx;
    const char *opt;
    if (lua_gettop(L) == 1) {
        entry = get_entry(get_codepoint(L, 1, 3));
        if (entry) {
            lua_pushinteger(L, entry->polyphone_count);
            return 1;
        }
        return 0;
    }
    entry = get_entry(ch = get_codepoint(L, 1, 4));
    idx = (int)luaL_checkinteger(L, 2);
    if (idx <= 0 || idx > entry->polyphone_count)
        return 0;
    opt = luaL_optstring(L, 3, NULL);
    if (idx == 1)
        return get_info(L, entry, opt);
    if (opt && *opt == 'p')
        return luaL_argerror(L, 3, "invalid option");
    entry = get_polyphone(ch);
    if (entry == NULL)
        return 0;
    return get_info(L, &entry[idx-2], opt);
}

static int Lindex(lua_State *L) {
    luaL_Buffer b;
    const PinyinIndex *pyidx;
    const char *stop, *s = luaL_checkstring(L, 1);
    PinyinEntry entry;
    int start, end;
    if ((stop = parse_pinyin(s, &entry)) == s)
        return 0;
    if ((pyidx = get_pyindex(entry)) == NULL)
        return 0;
    start = posrelat_raw(luaL_optinteger(L, 2, 1), pyidx->count);
    end = posrelat_raw(luaL_optinteger(L, 3, -1), pyidx->count);
    if (start > end) return 0;
    luaL_buffinit(L, &b);
    while (start <= end) {
        add_utf8char(&b, pyindex_data[pyidx->idx + start]);
        ++start;
    }
    luaL_pushresult(&b);
    if (*stop != '\0') {
        lua_pushstring(L, stop);
        return 2;
    }
    return 1;
}

LUALIB_API int luaopen_pinyin(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, L##name }
        ENTRY(pinyin),
        ENTRY(info),
        ENTRY(polyphone),
        ENTRY(index),
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
/* cc: flags+='-shared -s -O3 -DLUA_BUILD_AS_DLL' libs+='-llua53'
 * cc: output='pinyin.dll' run='lua test_py.lua'
 */
