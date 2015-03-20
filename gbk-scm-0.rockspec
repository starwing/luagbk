package = "gbk"
version = "scm-0"

source = {
  url = "git://github.com/starwing/luagbk.git"
}

description = {
  summary = "GBK support for Lua",
  detailed = [[
This module used to convert UTF-8 string from/to GBK encoded (Chinese
encoding) string.

It used database file from Unicode Database, see `parse_bestfit.lua` for more
detail.

It can be used to convert characters from other code page other than CP936
(the GBK code page), just download different bestfit file, and change the name
of library, but it only support mbcs encoding/decoding.
  ]],
  homepage = "http://github.com/starwing/luagbk",
  license = "MIT",
}

dependencies = {
  "lua >= 5.1"
}

build = {
  type = "builtin",
  modules = {
      gbk= "lgbk.c",
      pinyin= "lpinyin.c",
  }
}
