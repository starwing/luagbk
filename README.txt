GBK support for Lua
===================

This module used to convert UTF-8 string from/to GBK encoded (Chinese
encoding) string.

It used database file from Unicode Database, see parse_bestfit.lua for more
detail.

It can be used to convert characters from other code page other than CP936
(the GBK code page), just download different bestfit file, and change the name
of library, but it only support mbcs encoding/decoding.

It offer several routines to handle GBK encoding string:

    gbk.len(str) -> int
	return the length of a GBK encoded string.

    gbk.byte(s[, i[, j]]) -> characters...
	same as string.byte, but operates GBK encoded string.

    gbk.char(characters...) -> s
	same as string.char, but operates GBK encoded string.

    gbk.toutf8(gbk_str) -> utf8_str
	convert GBK encoded string to UTF-8 encoded string, using embedded
	conversion table.

    gbk.toutf8(characters...) -> characters...
	convert a list of GBK code point to Unicode code point, as integer.

    gbk.fromutf8(utf8_str) -> gbk_str
	same as gbk.toutf8, but convert from UTF-8 string to GBK string.

    gbk.fromutf8(characters...) -> characters...
	same as gbk.toutf8, but convert from UTF-8 code points to GBK code
	points.

This module has same license with Lua, e.g. the MIT license. You can use it
for any purpose.

for UTF-8 string handling, see my other module:
http://github.com/starwing/luautf8
