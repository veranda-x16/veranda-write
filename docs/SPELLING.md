# Spelling in Veranda Write

Veranda Write checks spelling in English and Polish (DECISIONS W56-W59).
The dictionaries, their format, the lookup and the suggestions are taken
over from **Writer**, the author's own earlier word processor for the X16
(its `src/spell`, `tools/build_dictionary.py`, `docs/dictionary-format.md`);
what Write changed is said below.

## On the card

| Path | What | Size |
|---|---|---|
| `/VERANDA/DICT/PL.DIC` | Polish: 300,000 word forms | 1,162,240 bytes |
| `/VERANDA/DICT/PL.DIC.txt` | its sources and licences (CC BY-SA 4.0; Morfologik's BSD notice) | 2,643 bytes |
| `/VERANDA/DICT/EN.DIC` | English: 165,326 word forms | 743,424 bytes |
| `/VERANDA/DICT/EN.DIC.txt` | its source and licence (SCOWL's) | 12,713 bytes |
| `/VERANDA/DICT/SPELL.OVL` | spelling's code, Write's overlays 23-26, loaded when spelling is first used; since 1.6 also the Word and OpenDocument code, overlays 17-22, loaded for an import or an export (must be from the same build as `Veranda Write.cvt`: it carries the build's mark) | about 67 KB |
| `/VERANDA/.SYSTEM/WRITE.UDIC` | the user's own words (made by Add; never shipped) | |
| `/VERANDA/.SYSTEM/WRITE.CFG` | the spelling settings (made when they are first changed; never shipped) | 7 bytes |

On a card without `/VERANDA` the dictionaries are looked for in `/DICT`.
Without them everything works but spelling, which says where they come
from (W61, below).

## Getting the dictionaries (W61)

The dictionaries are **not in Veranda's package** nor in its updates:
only `SPELL.OVL` is (it must match the program). They are the assets of
one GitHub release of this repository, **`dict-1`** (the tag is their
format: a later format gets `dict-2`, so a Write that reads format 1
never fetches one it cannot read; a new revision of the same format
replaces `dict-1`'s assets):

| Asset | What |
|---|---|
| `EN.DIC`, `EN.DIC.txt`, `PL.DIC`, `PL.DIC.txt` | the dictionaries and their licences, by their own names |
| `DICT.LST` | their list for Veranda Update (`UPDATE.LST`'s format, `VERANDA-UPDATE 1`: version, tag, each file's size, CRC-32 and place, `VERANDA/DICT/...`) |
| `Veranda-Write-dictionaries.zip` | the four files at `VERANDA/DICT/` and a README, to copy by hand |

`make dict-release` (`tools/mkdict.py dict build/dict-release`) makes
them and prints the `gh release create` / `gh release upload --clobber`
commands; the workflow `.github/workflows/dictionaries.yml` does the same
on GitHub when it is run (Actions > Dictionaries > Run workflow).

On the X16, **Tools > Get dictionaries** (`v/sppop.c`, overlay 26):
Write checks that `Veranda Update.cvt` is in `/VERANDA/APPS`, asks, then
writes two requests in `/VERANDA/.SYSTEM` and puts itself aside (as
Alt+Tab):

    START.REQ    /VERANDA/APPS/Veranda Update      (the desktop starts it
                                                    at its next start)
    UPDATE.REQ   dict                              (Veranda Update checks
                 back Veranda Write                 DICT.LST at once; its
                                                    Quit brings Write back)

Veranda Update (Veranda's DECISIONS V198) asks which dictionaries to get,
checks the card's room, fetches them with their CRC-32 checked into
`/VERANDA/DICT`, keeps a backup for Undo. Write needs no network code of
its own (its main RAM has 717 bytes free).

**Without a dictionary:** live marking looks once, finds none and stays
quiet (no marks, no box, no line: `dmiss`, not looked for again while
typing); Check spelling (F7) says "No English dictionary: Tools > Get
dictionaries, or copy EN.DIC to /VERANDA/DICT." in its panel; choosing a
language or turning the marks on says it in a box. F7, F8, a right click,
choosing a language and Get dictionaries look for the dictionary again
(`sp_drop(2)`), so one that arrives while Write runs (fetched, or copied)
is used from then on. Dictionaries left on a card by an earlier package
stay there and keep working: Veranda Update never deletes a file.

Spelling's code is not part of `Veranda Write.cvt`: Veranda loads a
program's overlays into RAM banks at its start for good, and four more
(32 KB) left a 512 KB machine too little memory to convert a large Word
document (DECISIONS W59). `SPELL.OVL` is read into four banks when
spelling is first used (live marking: the first pause after a document
is shown), and they and the dictionary's banks are given back before a
document is opened and before an export, then read again. Since 1.6
(W66) the same file also carries the code that opens and exports Word
and OpenDocument files (overlays 17-22): the tables' three overlays
wanted those banks, so it is read only for an import (four overlays)
or an export (three) and given back after. Without `SPELL.OVL` Write
runs, but has neither spelling nor .docx and .odt.

`WRITE.UDIC` is UTF-8, a word a line, lower case: it can be edited on any
computer. It holds the words of both languages (a name added while
writing Polish is known in English too).

## The settings (DECISIONS W60)

Tools > Spelling options opens a panel under the text (where Check
spelling's is):

- **Check spelling as you work** (on): off, spelling is used only when
  asked (F7, F8, the Tools menu): nothing is loaded or marked before,
  and a right click is not a word's suggestions.
- **Mark misspellings as you type** (on): the dotted lines. Shown ticked
  only while the first is on; ticking it turns the first on too. Tools >
  Mark misspellings changes the same setting and keeps it at once.
- **New documents**: English (the default, whatever the keyboard),
  Polish, or the keyboard's (Polish when Veranda's keyboard setting is
  Polish, else English): the language of a new document and of one saved
  without a language (CWRD 1.4 and older). A document's own language
  (Tools > English, Polish) is kept in it as before.

OK keeps them in `/VERANDA/.SYSTEM/WRITE.CFG` (in `/VERANDA` or the root
on a card without `.SYSTEM`); they are read when Write starts. The file:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `WCFG` |
| 4 | 1 | version, 1 (a later version only adds fields after these) |
| 5 | 1 | flags: bit 0 misspellings not marked, bit 1 spelling only when asked, bit 2 AI help off (W68: Tools > AI help; no AI menu); the others 0 |
| 6 | 1 | the language of new documents: 1 English (0 read as English too), 2 Polish, 3 the keyboard's |

A missing file, a shorter one, one that does not start `WCFG` or of
version 0 gives the defaults: everything on, English; a language it
does not know is English, flags it does not know are passed over.

## The dictionaries

**PL.DIC** is Writer's `assets/PL.DIC`, unchanged: the inflected forms of
Morfologik 2.1 "PoliMorf" (Marcin Miłkowski, BSD) that are most frequent
in the FrequencyWords Polish list (Hermit Dave, CC BY-SA 4.0), every form
of the common words (komputer, komputera, komputerem, komputerowi,
komputery, komputerów, komputerami). Writer measured 99.8 % of correctly
written running Polish found (one false alarm in 450 words). Rare words
are missing: "zażółć" and "gęślą" of the pangram are not in it. Rebuild:
Writer's `tools/fetch_morfologik.sh`.

**EN.DIC** is built by `tools/fetch_scowl.sh` with `tools/build_dictionary.py`
from SCOWL 2020.12.07 (Kevin Atkinson's Spell Checker Oriented Word
Lists, a permissive licence: `EN.DIC.txt`): the english-, american- and
british- words, upper (proper nouns), contractions and proper-names lists
at sizes 10 to 70, merged: American and British spellings ("color",
"colour"), inflections, possessives ("children's") and contractions
("don't", "o'clock"). On this repository's README and Writer's user guide
(6,172 words of technical English) 95 % of the words are found; the rest
are mostly the manuals' own terms (Ctrl, Alt, docx, geoWrite).

Both are in the Veranda set (ISO-8859-16), lower case: Write's text is in
that set whatever the language, so Writer's Polish dictionary needed no
change, and an English word with an accent ("café") is in it as written.

## The file format (Writer's, version 2)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `WRDC` |
| 4 | 1 | version, 2 |
| 5 | 1 | key size, 16 |
| 6 | 2 | blocks B |
| 8 | 4 | words |
| 12 | 2 | key pages P (Write takes 128 at most: 8,192 blocks, 8 MB) |
| 14 | 2 | the first key page's unit (file offset / 1024) |
| 16 | 2 | the first block's unit |
| 18 | 4 | the language, "PL" or "EN" |
| 64 | 16 P | the top index: each key page's first key |
| ... | 1024 P | key pages: 64 keys each, the first word of each block cut to 16 bytes |
| ... | 1024 B | blocks: words sorted bytewise, front-coded (shared prefix length, suffix length, suffix), `$FF` after the last |
| ... | 1024 | padding (`$FF`): no read reaches the file's end |

## A lookup

1. **The hash** (a RAM bank): every word looked up before, known or not,
   by two 16-bit sums of its letters; 512 sets of four entries, the most
   recent first; a known word is pushed out before an unknown one (the
   marks on the screen depend on those). A word met again costs no
   lookup (0.4 ms).
2. The user's words and the ignored ones (a RAM bank, searched).
3. **The dictionary**: the top index (in that bank) picks a key page, the
   key page a block, the block is scanned (Writer's `fast_scan`, in
   assembly). Key pages and blocks are 1 KB units of the file kept in a
   cache of RAM banks, the least recently used given up (Writer's went
   round; Writer had 16 KB). The cache's banks: a quarter of the free ones
   when spelling starts, 2 to 24, the first always and the others only
   while the document's pool keeps a bank to spare (typing uses it: at
   512 KB with a document open the cache is mostly one bank, 8 KB; at
   2 MB 24 banks, 192 KB of the dictionary); they go back to the pool
   before a chunk of the document would go out to the card. A unit's slot is found through a
   table of every unit, not a search. The file is opened when a unit must
   be read and stays open for the whole operation; a unit right after
   the last one read is read without a seek. A lookup that starts in the
   block the last one started in (the suggestions' candidates) skips both
   searches.

## Suggestions (Writer's `suggest.c`)

The candidates are made from the word and looked up, the likeliest
first: Polish letters put in or changed (a/ą, c/ć, e/ę, l/ł, n/ń, o/ó, s/ś,
z/ź/ż: "zolw" → "żółw", "ksiazka" → "książka"), the language's
confusions (Polish rz/ż, ch/h, u/ó, om/ą, on/ą, em/ę, en/ę; English ie/ei,
ant/ent, er/or, double letters, ph/f, a missing apostrophe: "dont" →
"don't"), two letters swapped ("recieve" → "receive"), a letter left out,
changed, added. At most 8, 1,200 lookups or 2.5 seconds. They are not
ordered by how common they are: "wich" offers "rich" before "which".

## The words of the text

Letters and digits; an apostrophe between letters belongs to the word; a
hyphen ends one ("well-known" is two words, each checked); style codes
inside a word are passed over. Checked: two letters or more, no digit, at
most 31 bytes. A word in capitals of at most 5 letters is an abbreviation
(NATO, PKP): not marked. "Zażółć" and "ZAŻÓŁĆ" are looked up as "zażółć".

## For the package

Veranda's package build (`tools/mkdist.sh` in the Veranda tree) builds
Write with `make veranda SDK=...`. That also puts spelling's code in
`build/veranda/DICT/SPELL.OVL` (about 67 KB: spelling's code and, since
1.6, the Word and OpenDocument code). The package copies it to
`/VERANDA/DICT/` on the card, from the same build as `Veranda Write.cvt`
(`SPELL.OVL` of another build is refused: no spelling). Since W61 the
dictionaries are not in the package (above: their own release);
`WRITE.UDIC` is made on the card when a word is added.
