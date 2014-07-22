io.input "Unihan_Readings.txt"
io.output "pinyin.h"

local uni_pinyin = {}

for line in io.lines() do
   if line:match "^#" then goto next end

   local cp, attr, value =
      line:match "^U%+(%x+)%s+(%w+)%s+(.*)$"
   if not cp then goto next end
   if attr ~= 'kHanyuPinlu' and
      attr ~= 'kHanyuPinyin' and
      attr ~= 'kMandarin' then
      goto next
   end

   cp = tonumber(cp, 16)

   local entry = uni_pinyin[cp]
   if not entry then
      entry = { cp = cp }
      uni_pinyin[cp] = entry
   end

   if attr == 'kHanyuPinlu' then
      for pinyin in value:gmatch "(%S+)%b()" do
         if not entry[pinyin] then
            local len = #entry+1
            entry[len] = pinyin
            entry[pinyin] = len
         end
      end
      goto next
   end

   if attr == 'kHanyuPinyin' then
      for list in value:gmatch "%:(%S+)" do
         for pinyin in list:gmatch "([^,]+)" do
            if not entry[pinyin] then
               local len = #entry+1
               entry[len] = pinyin
               entry[pinyin] = true
            end
         end
      end
      goto next
   end

   if attr == 'kMandarin' then
      local pinyin = value:match "%S+"
      entry.mandarin = pinyin
      if not entry[pinyin] then
         local len = #entry+1
         entry[len] = pinyin
         entry[pinyin] = len
      end
      goto next
   end

   ::next::
end

local normalize_pinyin do
local utf8patt = "[\xC2-\xF4][\x80-\xBF]*"
local rhyme = "āáǎàīíǐìūúǔùēéěèōóǒòüǖǘǚǜêếềḿńňǹǵǧ"
local nrhym = "aaaaiiiiuuuueeeeoooovvvvveeemnnngg"
local tone  = "1234123412341234123401234024223423"
local rhyme_combine = "{ê̄}{ê̌}{m̄}{m̌}{m̀}{n̄}{n̈}{l̈}{g̀}"
local nrhym_combine = "eemmmnnlg"
local tone_combine  = "131341004"

local tonemap = {}
local rhymemap = {}
local i = 1
for t in rhyme:gmatch(utf8patt) do
   rhymemap[t] = nrhym:sub(i,i)
   tonemap[t] = tonumber(tone:sub(i,i))
   i = i + 1
end

local combine = {}
local i = 1
for t in rhyme_combine:gmatch "{([^}]+)}" do
   combine[#combine+1] = t
   rhymemap[t] = nrhym_combine:sub(i,i)
   tonemap[t] = tonumber(tone_combine:sub(i,i))
   i = i + 1
end

function normalize_pinyin(py)
   local num = 0
   local function replace(s)
      if rhymemap[s] then
         num = tonemap[s]
         return rhymemap[s]
      end
   end
   for i = 1, #combine do
      local res, n = py:gsub(combine[i], replace)
      if n ~= 0 then
         py = res
         if num ~= 0 then
            return py, num
         else
            break
         end
      end
   end
   return py:gsub(utf8patt, replace), num
end

end

local sylmap, rhymap, get_pinyin_triple do

local syllable = "b|c|ch|d|f|g|h|j|k|l|m|n|p|q|r|s|sh|t|w|x|y|z|zh" -- 24
local rs = "a|ai|ao|an|ang|e|ei|en|eng|r|er|o|ou|m|n|ng"
local rhyme = [[
a|au|ai|ao|an|ang|
i|ia|iao|ian|iang|iu|ie|iong|in|ing|
u|ua|uai|uan|uang|ue|ui|uo|un|
e|ei|en|eng|er|r|
o|ou|ong|
v|ve|
m|
n|ng
]] -- 38

sylmap = {}
rhymap = {}

sylmap[""] = 1
sylmap[1] = ""
local i = 2
for s in syllable:gmatch "%w+" do
   sylmap[s] = i
   sylmap[i] = s
   i = i + 1
end

local i = 1
for r in rhyme:gmatch "%w+" do
   rhymap[r] = i
   rhymap[i] = r
   i = i + 1
end

local single = {}
for r in rs:gmatch "%w+" do
   single[r] = assert(rhymap[r])
end

function get_pinyin_triple(py)
   local npy, num = normalize_pinyin(py)
   if single[npy] then
      return 1, single[npy], num
   end
   local first, second = npy:match "([a-z]h?)([a-z]+)"
   return assert(sylmap[first]),
          assert(rhymap[second]),
          num
end

end

local function write_entry(idx, s, width)
   width = width or 8
   if idx % width == 0 then io.write "  " end
   io.write(s, ", ")
   if idx % width == (width-1) then io.write "\n" end
end

local function write_tables(prefix, codes, format, t)
   table.sort(codes)
   t = t or "unsigned short"
   local leaders = {}
   local leader
   local last_cp = 0
   for _, cp in ipairs(codes) do
      local cl = math.floor(cp/2^8)
      if cl ~= leader then
         if leader ~= nil then
            for i = last_cp + 1, (leader+1)*2^8-1 do
               write_entry(i, format())
            end
            io.write "};\n\n"
         end
         leaders[cl] = true
         leader = cl
         io.write(("static const %s %s_%02X[256] = {\n")
            :format(t, prefix, leader))
         last_cp = leader*2^8-1
      end
      for i = last_cp + 1, cp-1 do
         write_entry(i, format())
      end
      write_entry(cp, format(cp))
      last_cp = cp
   end
   for i = last_cp + 1, (leader+1)*2^8-1 do
      write_entry(i, format())
   end
   io.write "};\n\n"

   io.write(("static const %s *%s[256] = {\n"):format(t, prefix))
   for i = 0, 255 do
      local leader = leaders[i]
      if leader then
         write_entry(i, ("%s_%02X"):format(prefix, i))
      else
         write_entry(i, "0")
      end
   end
   io.write "};\n\n\n"
end

local function write_strings(t, width, indent)
   width = width or 78
   indent = indent or 2
   local s = { (" "):rep(indent) }
   local linewidth = indent
   for _, str in ipairs(t) do
      local written = ("%q, "):format(str):gsub("\n", "\\n")
      if linewidth + #written > width then
         s[#s+1] = "\n"..(" "):rep(indent)
         linewidth = indent
      end
      linewidth = linewidth + #written
      s[#s+1] = written
   end
   s[#s+1] = "\n"
   io.write(table.concat(s))
end


io.write [[
#ifndef pinyin_h
#define pinyin_h


static const char *py_syllables[64] = {
]]
write_strings(sylmap, 78)

io.write [[
};

static const char *py_rhymes[64] = {
]]
write_strings(rhymap, 78)


io.write [[
};


typedef struct PinyinEntry {
   char syllable;
   char rhyme;
   char tone;
   char polyphone_count;
} PinyinEntry;

typedef struct PinyinPolyphone {
   unsigned short cp;
   unsigned short idx;
} PinyinPolyphone;


]]

local codes = {}
for cp in pairs(uni_pinyin) do
   if cp < 0x10000 then
      codes[#codes+1] = cp
   end
end
write_tables("pytable", codes, function(cp)
   if not cp then return "{ 0, 0, 0,0}" end
   local entry = uni_pinyin[cp]
   local s,r,t = get_pinyin_triple(entry[1])
   return ("{%2d,%2d, %d,%d}"):format(s-1,r-1,t,#entry)
end, "PinyinEntry")


local polyphone = {}
for cp, entry in pairs(uni_pinyin) do
   if cp < 0x10000 and #entry > 1 then
      polyphone[#polyphone+1] = cp
   end
end
table.sort(polyphone)

local polyphone_idx = {}
local polyphone_data = {}
for _, cp in ipairs(polyphone) do
   local entry = uni_pinyin[cp]
   local idx
   local data = {}
   for i = 2, #entry do
      local s,r,t = get_pinyin_triple(entry[i])
      data[#data+1] = (s-1)*2^16+(r-1)*2^8+t
   end
   for i = 1, #polyphone_data do
      if polyphone_data[i] ~= data[1] then
         goto next
      end
      for ii, v in ipairs(data) do
         if polyphone_data[i+ii-1] ~= v then
            goto next
         end
      end
      idx = i
      break
      ::next::
   end
   if not idx then
      idx = #polyphone_data+1
      for i, v in ipairs(data) do
         polyphone_data[idx+i-1] = v
      end
   end
   polyphone_idx[#polyphone_idx+1] = {
      cp = cp,
      idx = idx-1,
   }
end

io.write([[
#define POLYPHONE_COUNT ]]..#polyphone_idx..[[


static const PinyinPolyphone polyphone[] = {
]])
for i, v in ipairs(polyphone_idx) do
   write_entry(i-1, ("{0x%04X,%d}"):format(v.cp, v.idx))
end

io.write [[
};

static const PinyinEntry polyphone_data[] = {
]]
for i, v in ipairs(polyphone_data) do
   local s,r,t = math.floor(v/2^16),
                 math.floor(v/2^8)%2^8,
                 v%2^8
   write_entry(i-1, ("{%2d,%2d, %d,%d}"):format(
      s,r,t,0))
end

io.write [[
};


#endif /* pinyin_h */
]]

do return end

local counts = {}
for _,entry in pairs(uni_pinyin) do
   if not counts[#entry] then
      counts[#entry] = 1
   else
      counts[#entry] = counts[#entry]+1
   end
end
for i = 1, 12 do
   if counts[i] then
      print(i, counts[i])
   end
end

local pymap = {}
for _,entry in pairs(uni_pinyin) do
   for _,v in ipairs(entry) do
      pymap[v] = true
   end
end

local sorted_py = {}
local normmap = {}
for k in pairs(pymap) do
   sorted_py[#sorted_py+1] = k
   local npy, num = normalize_pinyin(k)
   normmap[k] = npy..(num == 0 and '' or num)
end
table.sort(sorted_py, function(a,b)
   return normmap[a] < normmap[b]
end)
io.output "pymap.txt"
for _, v in ipairs(sorted_py) do
   local f,s,n = get_pinyin_triple(v)
   io.write(v, ("(%s %d,%d,%d)"):format(normmap[v], f,s,n), "\n")
end
