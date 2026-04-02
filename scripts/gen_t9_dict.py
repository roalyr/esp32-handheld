#!/usr/bin/env python3
"""
Generate T9 predictive dictionary C header from a word list.

Usage:
    python3 scripts/gen_t9_dict.py [wordlist.txt] > src/t9_dict.h

If no wordlist is provided, uses /usr/share/dict/words with a built-in
frequency ranking of the ~3000 most common English words.

The output is a C header with PROGMEM arrays for ESP32 Arduino.
"""

import sys
import os
from collections import defaultdict

LETTER_TO_DIGIT = {
    'a': '2', 'b': '2', 'c': '2',
    'd': '3', 'e': '3', 'f': '3',
    'g': '4', 'h': '4', 'i': '4',
    'j': '5', 'k': '5', 'l': '5',
    'm': '6', 'n': '6', 'o': '6',
    'p': '7', 'q': '7', 'r': '7', 's': '7',
    't': '8', 'u': '8', 'v': '8',
    'w': '9', 'x': '9', 'y': '9', 'z': '9',
}

# Top ~3000 English words by approximate frequency.
# Used when no external wordlist is provided.
# Words must be lowercase, alphabetic only.
BUILTIN_WORDS = """
the of and to a in is it you that he was for on are with as his they be at one
have this from or had by not but what all were we when your can said there use
an each which she do how their if will up other about out many then them these
so some her would make like him into time has look two more write go see number
no way could people my than first water been call who oil its find long down day
did get come made may part over new after also back just year came show every
good me give our under name very through much before line right too mean old any
same tell boy follow want world still large last us should here thing help home
big high think place small end turn just put read hand large spell add even land
such because follow why ask men change went light kind off need house picture try
us again animal point page letter mother answer found study learn one know
live try together next white children begin got walk example ease paper often
run late real almost let open seem side sea talk begin thought story saw far
hand high head start might close something enough eat face watch far real let
open seem together next white children begin got walk example ease paper life city
tree cross since hard start might close something walk thought feet care second
book carry took science room friend idea fish mountain stop once base hear horse
cut eye color door red food body dog game hot miss green sun work blue top
above keep school never last once while fire left main play home read hand
own order found hold old along car door four five six seven eight nine ten
young love family keep leave body music food half stop song being room money
book game fire office move power water form morning class want left read sound
problem find ask talk week sure job better system change help three show hand
keep going run meet pay think again turn start need home going end take study
start say get try know help old set well come might come want part thing look
good give most find year work feel turn change run hold leave house point play
part turn just big still high play face place old last long great little own just
good come use work like time really way find right call back give most other stay
young war state think old world should still own great little country hand keep
eye face word sure before want come really work well study line change head hand 
keep turn life look old play end state same always place point form between each
large must big even other end through such after important different man her
light home hand word between small story world head near build self earth father
night sentence take point mother left girl four our live three right long boy
side old game again few state let hard open get start long give many any same
run look only side set try ask men year around move our away big write may
long about before too after line name right say give some long help tell mean
boy old follow came want show also around farm set large change end spell
add land here home us hand page letter mother answer study learn animal point
story know live walk example ease paper often run late real head start
might read near something enough eat face watch far real let open long think
seem group together next white children begin got tree cross since hard play
enough start stop once base hear horse cut sure move eye color door full red
body dog game hot miss green sun money blue young must girl job music stay
food great half stop way side year boy song being room along main fire left
below keep school never last always let state once while sure better along
each line kind class between important off need system work picture full part
without place turn again show change should age state after number long city
world just few leave left put last night second end great school head still
above idea house own hard school stand start feel game form family right long
hard need way start feel country house place close eye back take study set let
plan four five six second late school order side left put night end right play time
state learn run world group change light end close hand form important something
part point such place still between never put just most start same good head home
live give try came say ask men turn state think change help show know find work
get call look part most right just long still good back come all well
think did right also back use find could any just small some large
about before long right tell home hand keep play eye face state let
once end help long old such start might head near let close meet something
point enough eat face watch far city long let hold seem kind together
always next white children begin got walk example ease paper run late
under real almost let near start might close enough leave once run
together next white children begin got walk example ease paper run late real
main almost red build green blue stop side young music girl keep school
family money state better job people country big city run small new tell
run hand near start might close form something enough eat
able above across act add age ago agree air almost along already
always among amount animal answer any appear area arm around art ask
at away bad ball bank base be bear beat beautiful become bed before began
begin behind believe below beside best better between big bird bit black
blood blue board body bone book both bottom box boy brain bring
brother build burn bus business busy buy
call came can capital car care carry case cat catch
cause center certain chance change character charge check child
choice choose church circle city class clean clear close
cold come common company complete condition consider contain
continue control cook cool corner cost could country course
cover create cross cry current cut
dance dark daughter day dead dear decide deep develop did die
different difficult dinner direction discover do doctor does dog
dollar done door down draw dream dress drink drive drop dry during
each ear early earth east eat edge education effect egg eight either
else employee end enemy energy enjoy enough enter especially even
evening ever every example excite exercise expect experience explain
face fact fair fall family far fast father feel feet few field fight
fill final find fine finger finish fire first fish five flat floor
fly follow food foot for force foreign forest forget form forward
found four free fresh friend from front fruit full fun future
game garden gas gather general get girl give glad glass go god
gold gone good got govern grass great green grew ground group grow
guess gun guy had hair half hand happen happy hard has hat have
he head hear heart heat heavy help her here high hill him his hit
hold hole home hope horse hot hotel hour house how however huge
human hundred hunt hurry husband
ice idea if imagine important in include increase indicate industry
information inside instead interest into iron island issue it its
itself job join joy jump just keep key kid kill kind king kitchen
knew know land language large last late later laugh law lay lead
learn least leave left leg less let letter level lie life light like
line list listen little live long look lose lot love low
machine main major make man manage many mark market matter may me
mean measure meet member memory men might million mind mine minute
miss modern moment money month more morning most mother move much
music must my name nation nature near necessary need never new news
next nice night nine no none nor north nose not note nothing notice
now number occur of off offer office often oh oil old on once one
only open operate or order other our out outside over own page paint
pair paper parent part particular party pass past pay people per
perhaps period person pick piece place plan plant play please point
poor position possible power prepare present president pretty
probably problem produce product program provide public pull purpose
push put quality question quite radio raise range rather read ready
real reason receive record red remain remember remove reply report
rest result return rich right rise risk road rock role room rule run
safe same save say scene school science score sea season seat second
see seem sell send sense serve set seven several shake shall shape
share she shoot short should shoulder show side sign simple since
sing sister sit situation six size skill skin small so social some
son soon sort sound south space speak special spend sport spring
stand star start state stay step still stock stop story strategy
street strong student study stuff subject success such suddenly
suffer suggest summer support sure surface system table take talk
task teach team tell ten tend term test than thank that the their
them then there these they thing think third this those though
thought thousand three through throw time to today together too top
total tough toward town trade tree trial trip trouble true truth try
turn tv two type
under understand unit until up upon us use usually value very
visit voice vote wait walk wall want war watch water way we weapon
wear week weight well west western what whatever when where whether
which while white who whole whom whose why wide wife will win wind
window wish with within without woman wonder word work worker world
worry would write wrong
yard yeah year yes yet you young your yourself youth
""".split()


def word_to_digits(word):
    """Convert a word to its T9 digit sequence."""
    digits = []
    for ch in word.lower():
        if ch in LETTER_TO_DIGIT:
            digits.append(LETTER_TO_DIGIT[ch])
        else:
            return None
    return ''.join(digits)


def digits_to_key(digits):
    """Convert digit string to uint64 sort key.
    
    Left-aligned in a 15-digit field so that prefix ordering is
    preserved in integer sort.  e.g. "7663" -> 766300000000000.
    Max 15 digits supported.
    """
    key = 0
    for i, d in enumerate(digits[:15]):
        key = key * 10 + int(d)
    # Left-align: pad to 15 digits
    key *= 10 ** (15 - len(digits[:15]))
    return key


def generate_dict(words, max_candidates_per_seq=8, max_word_len=15):
    """Group words by T9 digit sequence and build index."""
    
    # De-duplicate while preserving order (first occurrence wins = highest freq)
    seen = set()
    unique_words = []
    for w in words:
        w = w.lower().strip()
        if w and w.isalpha() and len(w) <= max_word_len and w not in seen:
            seen.add(w)
            unique_words.append(w)
    
    # Group by digit sequence
    groups = defaultdict(list)
    for word in unique_words:
        digits = word_to_digits(word)
        if digits and len(digits) <= 15:
            if len(groups[digits]) < max_candidates_per_seq:
                groups[digits].append(word)
    
    # Sort groups by digit key
    sorted_groups = sorted(groups.items(), key=lambda x: digits_to_key(x[0]))
    
    return sorted_groups


def emit_header(sorted_groups, out=sys.stdout):
    """Emit C header with PROGMEM dictionary data."""
    
    total_words = sum(len(ws) for _, ws in sorted_groups)
    
    # Build word pool
    word_pool = bytearray()
    index_entries = []
    
    for digits, word_list in sorted_groups:
        key = digits_to_key(digits)
        offset = len(word_pool)
        count = len(word_list)
        for w in word_list:
            word_pool.extend(w.encode('ascii'))
            word_pool.append(0)
        index_entries.append((key, offset, count))
    
    out.write('// AUTO-GENERATED — do not edit. Run: python3 scripts/gen_t9_dict.py > src/t9_dict.h\n')
    out.write(f'// {len(index_entries)} digit sequences, {total_words} words, {len(word_pool)} bytes word pool\n')
    out.write('#ifndef T9_DICT_DATA_H\n')
    out.write('#define T9_DICT_DATA_H\n')
    out.write('#include <Arduino.h>\n\n')
    
    # Index entry struct
    out.write('struct T9IndexEntry {\n')
    out.write('    uint64_t key;      // Digit sequence as decimal (e.g., "4663" = 4663)\n')
    out.write('    uint32_t offset;   // Byte offset into t9_word_pool\n')
    out.write('    uint8_t  count;    // Number of words for this sequence\n')
    out.write('    uint8_t  _pad[3];\n')
    out.write('};\n\n')
    
    # Word pool
    out.write(f'const char t9_word_pool[{len(word_pool)}] PROGMEM = {{\n')
    for i in range(0, len(word_pool), 16):
        chunk = word_pool[i:i+16]
        hex_vals = ', '.join(f'0x{b:02X}' for b in chunk)
        # Add ASCII comment
        ascii_repr = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        out.write(f'    {hex_vals},  // {ascii_repr}\n')
    out.write('};\n\n')
    
    # Index
    out.write(f'const T9IndexEntry t9_index[{len(index_entries)}] PROGMEM = {{\n')
    for key, offset, count in index_entries:
        # Recover digit string for comment
        digit_str = str(key)
        # Find first word for context
        first_word = ''
        for b in word_pool[offset:offset+30]:
            if b == 0:
                break
            first_word += chr(b)
        out.write(f'    {{{key}ULL, {offset}, {count}, {{0}}}},')
        out.write(f'  // "{digit_str}" -> "{first_word}"...\n')
    out.write('};\n\n')
    
    out.write(f'const uint32_t T9_INDEX_COUNT = {len(index_entries)};\n')
    out.write(f'const uint32_t T9_WORD_POOL_SIZE = {len(word_pool)};\n\n')
    out.write('#endif // T9_DICT_DATA_H\n')


# Lua keywords and standard library identifiers useful for T9 prediction
LUA_WORDS = """
and break do else elseif end false for function goto if in local nil not or
repeat return then true until while
assert error ipairs load next pairs pcall print require select type xpcall
tonumber tostring rawequal rawget rawlen rawset
setmetatable getmetatable collectgarbage dofile loadfile
byte char dump find format gmatch gsub len lower match rep reverse sub upper
concat insert move pack remove sort unpack
abs acos asin atan ceil cos deg exp floor fmod huge log max min modf rad
random seed sin sqrt tan
close flush input lines open output read write tmpfile
clock date difftime execute exit getenv rename
coroutine create resume wrap yield status running
debug getinfo getlocal getupvalue sethook setlocal setupvalue
traceback
""".split()


def main():
    if len(sys.argv) > 1 and os.path.isfile(sys.argv[1]):
        # Read external word list (one word per line, frequency sorted)
        with open(sys.argv[1]) as f:
            words = [line.strip().lower() for line in f if line.strip()]
        words.extend(LUA_WORDS)  # Always include Lua keywords
        print(f'// Source: {sys.argv[1]} + lua ({len(words)} input words)', file=sys.stderr)
    else:
        # Use built-in frequency list + Lua keywords
        words = list(BUILTIN_WORDS)
        words.extend(LUA_WORDS)
        print(f'// Source: built-in + lua ({len(words)} input words)', file=sys.stderr)
    
    sorted_groups = generate_dict(words)
    emit_header(sorted_groups)
    
    total_words = sum(len(ws) for _, ws in sorted_groups)
    print(f'// Generated: {len(sorted_groups)} sequences, {total_words} words', file=sys.stderr)


if __name__ == '__main__':
    main()
