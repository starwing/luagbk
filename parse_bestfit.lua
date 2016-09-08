-- download bestfit936.txt from:
-- http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit936.txt
local CP      = arg[1] or "936"
local NAME    = arg[2] or "gbk"
local INVALID = tonumber(arg[3] or 0xFFFE)
io.input("bestfit"..CP..".txt")
io.output(NAME..".h")

local info = {}
local cp_codes = {}
local uni_codes = {}
local to_cp = {}
local from_cp = {}

local linum = 0
local mb_count
local dbcs_count
local dbcs_range
local dbcs_table
local wc_count
for line in io.lines() do
   linum = linum + 1
   if line:match "^%s*$" then goto next end
   line = line:gsub("%s*;.*$", "")

   if mb_count then
      local cp_code, uni_code = 
         line:match "0x(%x+)%s+0x(%x+)"
      if cp_code then
         cp_code = tonumber(cp_code, 16)
         uni_code = tonumber(uni_code, 16)
         from_cp[cp_code] = uni_code
         cp_codes[#cp_codes + 1] = cp_code
         mb_count = mb_count - 1
         if mb_count == 0 then mb_count = nil end
         goto next
      end
   end

   if dbcs_table then
      local cp_code, uni_code =
         line:match "0x(%x+)%s+0x(%x+)"
      if cp_code then
         cp_code = tonumber(cp_code, 16)
         uni_code = tonumber(uni_code, 16)
         cp_code = cp_code + dbcs_range.current*2^8
         from_cp[cp_code] = uni_code
         cp_codes[#cp_codes + 1] = cp_code
         dbcs_table = dbcs_table - 1
         if dbcs_table == 0 then
            dbcs_table = nil
            if dbcs_range.current == dbcs_range.last then
               dbcs_count = dbcs_count - 1
               if dbcs_count == 0 then dbcs_count = nil end
               dbcs_range = nil
            end
         end
         goto next
      end
   end

   if wc_count then
      local uni_code, cp_code =
         line:match "0x(%x+)%s+0x(%x+)"
      if uni_code then
         uni_code = tonumber(uni_code, 16)
         cp_code = tonumber(cp_code, 16)
         to_cp[uni_code] = cp_code
         uni_codes[#uni_codes + 1] = uni_code
         wc_count = wc_count - 1
         if wc_count == 0 then wc_count = nil end
         goto next
      end
   end

   local cp = line:match "CODEPAGE%s+(%d+)"
   if cp then
      info.cp = tonumber(cp)
      goto next
   end

   local endcodepage =
      line:match "ENDCODEPAGE"
   if endcodepage then
      break
   end

   local dbcs, def_cp, def_uni =
      line:match "CPINFO%s+(%d+)%s+0x(%x+)%s+0x(%x+)"
   if dbcs then
      info.dbcs, info.def_cp, info.def_uni =
         tonumber(dbcs),
         tonumber(def_cp, 16),
         tonumber(def_uni, 16)
      goto next
   end

   local mbtable =
      line:match "MBTABLE%s+(%d+)"
   if mbtable then
      mb_count = tonumber(mbtable)
      --print("mbtable: ", mb_count)
      goto next
   end

   local dbcsrange = 
      line:match "DBCSRANGE%s+(%d+)"
   if dbcsrange then
      dbcs_count = tonumber(dbcsrange)
      --print("dbcs_count: ", dbcs_count)
      goto next
   end

   if dbcs_count and not dbcs_range then
      local first, last =
         line:match "0x(%x+)%s+0x(%x+)"
      first = tonumber(first, 16)
      last = tonumber(last, 16)
      dbcs_range = {
         first = first,
         last = last,
         current = first - 1
      }
      --print("dbcs_range: ", first, last)
      goto next
   end

   if dbcs_range then
      local dbcstable = 
         line:match "DBCSTABLE%s+(%d+)"
      if dbcstable then
         dbcs_table = tonumber(dbcstable)
         dbcs_range.current = dbcs_range.current + 1
         --print("dbcs_table: ", dbcs_table, dbcs_range.current)
         goto next
      end
   end

   local wctable =
      line:match "WCTABLE%s+(%d+)"
   if wctable then
      wc_count = tonumber(wctable)
      --print("wc_count:", wc_count)
      goto next
   end

   error(linum..': '..line)
   ::next::
end

local function output_code(idx, cp)
   if idx % 8 == 0 then io.write "  " end
   if type(cp) == 'number' then
      io.write(("0x%04X, "):format(cp))
   else
      io.write(tostring(cp), ", ")
   end
   if idx % 8 == 7 then io.write "\n" end
end

local function write_tables(prefix, codes, maps, def)
   def = def or INVALID
   table.sort(codes)
   local leaders = {}
   local leader
   local last_cp = 0
   for _, cp in ipairs(codes) do
      local cl = math.floor(cp/2^8)
      if cl ~= leader then
         if leader ~= nil then
            for i = last_cp + 1, (leader+1)*2^8-1 do
               output_code(i, def)
            end
            io.write "};\n\n"
         end
         leaders[cl] = true
         leader = cl
         io.write(("static const unsigned short %s_%02X[256] = {\n")
            :format(prefix, leader))
         last_cp = leader*2^8-1
      end
      for i = last_cp + 1, cp-1 do
         output_code(i, def)
      end
      output_code(cp, assert(maps[cp]))
      last_cp = cp
   end
   for i = last_cp + 1, (leader+1)*2^8-1 do
      output_code(i, def)
   end
   io.write "};\n\n"

   io.write(("const unsigned short *%s[256] = {\n"):format(prefix))
   for i = 0, 255 do
      local leader = leaders[i]
      if leader then
         output_code(i, ("%s_%02X"):format(prefix, i))
      else
         output_code(i, "NULL")
      end
   end
   io.write "};\n\n\n"
end

io.write(([[
#ifndef ]]..NAME..[[_h
#define ]]..NAME..[[_h

#include <stddef.h>

#define DBCS_DEFAULT_CODE     %#X
#define UNI_DEFAULT_CODE      %#X
#define UNI_INVALID_CODE      %#X

]]):format(info.def_cp, info.def_uni, INVALID))

write_tables("to_uni", cp_codes, from_cp)
write_tables("from_uni", uni_codes, to_cp, info.def_cp)



io.write("#endif /* "..NAME.."_h */\n")
