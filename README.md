GBK support for Lua
===================

This module used to convert UTF-8 string from/to GBK encoded (Chinese
encoding) string.

It used database file from Unicode Database, see `parse_bestfit.lua` for more
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


Pinyin support for UTF-8 Chinese character
------------------------------------------

When you have UTF-8 string chinese, you can use another module in this repo:
pinyin to get the 汉语拼音(Hanyu Pinyin) of Chinese text. this module's data
are from
[Unicode Character Database](http://www.unicode.org/Public/UCD/latest/ucd/).
a script `parse_unihan_readings.lua`
will parse text file `Unihan_Readings.txt` from `Unihan.zip` archive in UCD to
produce `pinyin.h` header file.

It offers several routines to convert UTF-8 Chinese text to pinyin:

    pinyin.pinyin(text, opts[, start[, end]])
        convert text to Pinyin, if `opts` is `"tone"`, the Pinyin will append
        numbers as the tone of Pinyin, Such as `han4 yu3 pin1 yin1`, if `opts`
        is `"utf8"`, it will produce utf8 Pinyin, Such as `hàn yǔ pīn yīn`.

        you can use optional `start` and `end` to just convert a sub of text
        string.

    pinyin.info(text, opts[, start[, end]])
        get Pinyin information from text. opts has several meanings:
        - `"syllable"`, get the syllable pinyin from text, such as `h` in `han`
        - `"rhyme"`, get the rhyme pinyin from text, such as `an` in `han`
        - `"tone"`, get the tone of pinyin, such as 2 in `han2`
        - `"polyphone"`, get the count of different voice with the same
          character, such as 还 have three readings: hai2, huan2, and fu2.
        - `"utf8"`, get the UTF-8 toned pinyin, such as `hàn`
        - `"utf8rhyme"`, get the UTF-8 toned rhyme, such as `àn`

    pinyin.polyphone(text[, idx[, opts[, start[, end]]]])
        get the polyphone information from character in text. opts has the
        same meaning in `pinyin.info`, but notice that the `"polyphone"`
        option is invalid. 
        if call with text only, return the count of polyphone.
        if call with a idx, return the idx-th pinyin of that character.
        if idx is out of bound, return nil.


This module has same license with Lua, e.g. the MIT license. You can use it
for any purpose.

for UTF-8 string handling, see my other module:
http://github.com/starwing/luautf8

<!-- vim: set ft=markdown ai nu et sw=4 :-->
