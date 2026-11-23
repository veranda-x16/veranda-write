# Veranda Write's document format (CWRD)

Version 1.3 (1.0 plus paragraph styles: the `PARA` chunk and the `$11`
code, 1.1; the page break, the `$12` code, 1.2; the text's character set
in `INFO`, 1.3). A Veranda Write document is a plain file on the card (any
FAT name; the program adds `.cwr` when a name has no extension, and
Veranda's shell opens `.CWR` files with it). All numbers are little
endian. `src/format.c` reads and writes it on the X16 and in the host
tests; `tools/cwrd.py` does the same on a PC.

## The header (8 bytes)

| Offset | Size | Contents |
|---|---|---|
| 0 | 4 | `CWRD` |
| 4 | 1 | major version: 1 |
| 5 | 1 | minor version: 3 (1.0's files: 0, 1.1's: 1, 1.2's: 2) |
| 6 | 2 | flags: 0 (reserved) |

A reader refuses a file whose major version is newer than its own. A
newer minor version only adds chunks (and code kinds, below): a reader
skips what it does not know and keeps the rest.

## Chunks

After the header, chunks until `END `: each is a tag of 4 ASCII
characters, a 32-bit length, then that many bytes. Unknown tags are
skipped. The order matters only where noted.

| Tag | Since | Contents |
|---|---|---|
| `INFO` | 1.0 | 4 bytes: the cursor (a place in the text); 2: the text's width in pixels (520: 6.5 inches at 80 pixels an inch); 1: the text's character set (1.3; reserved, 0, before): 0 Latin-1 (ISO-8859-1), 1 ISO-8859-16 (Veranda's set); 1: reserved (0). Longer in later versions: read the first 8 bytes, skip the rest |
| `FONT` | 1.0 | 1 byte: n, then n names: a length byte and the name (Latin-1, at most 31 characters): the fonts the styles use, by the names Veranda's `LoadFont` takes ("Gate Sans", "System") |
| `STYL` | 1.0 | 1 byte: n (1-128 in 1.0), then n character styles of 4 bytes: the font (an index into `FONT`), the size in points, the flags, the colour (0; reserved for later) |
| `PARA` | 1.1 | 1 byte: n (1-32), then n paragraph styles of e bytes each, e = (the chunk's length - 1) / n: 8 in 1.1 (below); a later version may make them longer: read the first 8 of each. Style 0 is the style of every paragraph without a paragraph code. Must come before `TEXT`. Without it (a 1.0 file) there is style 0 only: Body |
| `TEXT` | 1.0 | the text: the document's byte stream (below). Must come after `FONT`, `STYL` and `PARA` |
| `END ` | 1.0 | length 0: the end |

Reserved for later versions (stage 3 and on): `PAGE` (the page: size,
margins, headers and footers, page numbers), `NOTE` (footnotes), `OBJS`
(tables, pictures), `META` (title, author, dates).

A paragraph style (8 bytes; pixels at 80 an inch, as the text's width):

| Offset | Size | Contents |
|---|---|---|
| 0 | 1 | the kind (the named style it is, or was made from): 0 Body, 1-3 Heading 1-3, 4 Quote, 5 Bulleted list, 6 Numbered list; a reader takes another as 0 |
| 1 | 1 | the alignment: 0 left, 1 centred, 2 right, 3 justified (the paragraph's last line left); another: 0 |
| 2 | 1 | the left indent |
| 3 | 1 | the right indent |
| 4 | 1 | the first line's indent from the left one, signed (a list: where its bullet or number goes; the list's text starts at the left indent on every line) |
| 5 | 1 | the space above the paragraph's first line |
| 6 | 1 | the space below its last line |
| 7 | 1 | the line spacing in halves of a line: 2 single, 3 one and a half, 4 double; another: 2 |

A list's bullet or number is drawn by the program, not kept in the text;
a numbered paragraph is numbered 1 plus the number of the numbered
paragraph straight before it (1 when the one before is not numbered).

Style flags: bit 0 bold, bit 1 italic, bit 2 underline; bits 3-7 are
reserved (outline, superscript, subscript, strike-through ...): a 1.0
reader keeps them and draws the style without them.

## The text

One byte stream, the same as the program keeps in its RAM banks:

| Bytes | Meaning |
|---|---|
| `$20-$7E`, `$A0-$FF` | characters: ASCII, and above it the set `INFO` names (below) |
| `$09` | a tab |
| `$0D` | the end of a paragraph |
| `$10-$17` | a 3-byte code: kind, argument, kind |
| `$18-$1F` | reserved: 5-byte codes (kind, 3 bytes, kind) in a later version |
| anything else | not allowed (a reader drops it) |

**The character set** (1.3). Bytes `$A0-$FF` are ISO-8859-16 (Latin-10:
Polish's Ą Ć Ę Ł Ń Ó Ś Ź Ż and their small letters, Romanian, Hungarian,
Croatian, German, French, the euro), the set every Veranda program uses
since Veranda's V135, when `INFO`'s byte 6 is 1: what Veranda Write
writes. A file of 1.0-1.2 (byte 6 is 0) is Latin-1, its text as the
program kept it before; reading it, Veranda Write converts it: 56 of
the 96 characters `$A0-$FF` are the same in both, the other 40 (Latin-1's
¡ ¢ £ ¤ ¥ ¦ ¨ ª ¬ ® ¯ ² ³ ´ µ ¸ ¹ º ¼ ½ ¾ ¿ Ã Å Ð Ñ Õ × Ø Ý Þ ã å ð ñ õ ÷ ø ý þ)
become `?`, counted and said in the status bar; saved again it is 1.3. A
1.0-1.2 program reads a 1.3 file's text as Latin-1 (Polish letters show
as other Latin-1 characters; nothing is lost while it keeps them).
Plain text files open as UTF-8 (its characters that the set has not:
`?`, counted), a byte that is not UTF-8 as Latin-1, converted as above.

A code reads the same from either end (its first and last byte are its
kind), so a program can walk the text backwards. Kinds:

| Kind | Since | Argument |
|---|---|---|
| `$10` | 1.0 | character style: the characters after it, up to the next `$10` code, are in that style (an index into `STYL`) |
| `$11` | 1.1 | paragraph style: the paragraph's style, an index into `PARA` (1 .. n - 1: style 0 needs no code). Only as the first bytes of a paragraph (the text's first, or straight after a CR), before any other code |
| `$12` | 1.2 | a page break: argument 0. A paragraph of its own: `[$12 0 $12]` and the paragraph's CR, nothing else in the paragraph (no paragraph code). The next paragraph starts a new page (stage 3 makes pages; stage 2 draws a dashed line across the column there). A page break in a paragraph with anything else in it does nothing (Veranda Write takes it out when a deletion next to it, Backspace or Delete, joins its paragraph to another; text typed into its paragraph leaves it there, doing nothing, and hides the line). A 1.1 reader keeps it and shows an empty paragraph |
| `$13` | (5) | an object: an index into `OBJS` (a table, a picture) |
| `$14-$17` | | reserved |

The text starts in style 0. A reader drops a character style code whose
argument is not a style of `STYL`, a paragraph code whose argument is 0 or
not a style of `PARA` (so a 1.1 file saved by a 1.0 program, which keeps
the codes but not `PARA`, opens with its paragraphs in Body), and codes of
kinds it does not know are kept and passed over (they have no width): a
1.0 reader lays out a 1.1 file's paragraphs as Body.

Veranda Write writes no code that does nothing (it takes out a style
code straight before another one, one that sets the style already in
effect, one at the very end; it writes no paragraph code for style 0),
but a reader must not rely on that.

## geoWrite documents

Veranda Write opens geoWrite documents (GEOS record files of class
`Write Image V1.x` or `V2.x`, `.cvt` files on the card) by converting
them to a document of this format (`src/gwimport.c`, `tools/gwimport.py`;
DECISIONS W24-W28). The original is never written: Save writes a `.cwr`.

| geoWrite | Veranda Write |
|---|---|
| records 0-60 (the pages), in order | one text; a page's end inside a paragraph joins the paragraph (geoWrite's own break, the page full) |
| characters `$20-$7E`, TAB, CR | the same |
| PAGE_BREAK (`$0C`) between paragraphs | a page break (`$12`, its own paragraph) |
| any other byte (`$7F`, `$80-$FF`, unknown control codes) | `?`, counted and reported |
| `$00` | the page's end |
| NEWCARDSET (`$17`, the font's id word: id << 6 \| size; the style) | a character style: the font's name by its id (0 `System`, 1 `University`, 2 `California`, 3 `Roma`, Veranda's 300 `Gate Sans` and 301 `Gate Mono`, another `GEOS font n`) and its size kept as they are (a font not in `/VERANDA/FONTS` is drawn in Gate Sans at its nearest size, W7); bold; italic; underline; outline as bold; reverse, superscript and subscript as plain (reported) |
| ESC_RULER (`$11`, 26 bytes) | a paragraph style (Body's kind) for the paragraph it starts and those after it until the next ruler: the left margin the left indent, the right margin the right indent from the page's right edge (a page 480 dots wide, 640 when the margin is past 479), the paragraph margin the first line's indent, the alignment (bits 0-1), the line spacing (bits 2-3: single, 1.5, double); indents at most 200 pixels, the first line's -left .. 120; tab stops are not kept (Veranda Write's are every half inch; reported when a ruler had one between its margins) |
| ESC_GRAPHICS (`$10`, a picture of records 64-126) | the text `[picture]`, a paragraph of its own (pictures come with stage 5; reported) |
| records 61 and 62 (the header and the footer) | left out (reported) |
| the info block's page settings (first page number, title page, NLQ, the header's and footer's heights, the page's height) | left out |

## An example

"Hi **you**" and a paragraph end, centred, in Gate Sans 12:

```
43 57 52 44 01 01 00 00                     CWRD 1.1 (1.2 writes 01 02, 1.3 01 03)
49 4E 46 4F 08 00 00 00  00 00 00 00 08 02 00 00    INFO: cursor 0, width 520 (1.3: 08 02 01 00, ISO-8859-16)
46 4F 4E 54 0B 00 00 00  01 09 "Gate Sans"          FONT: 1 font
53 54 59 4C 09 00 00 00  02 00 0C 00 00 00 0C 01 00 STYL: 2 styles (plain, bold)
50 41 52 41 11 00 00 00  02                         PARA: 2 paragraph styles:
                         00 00 00 00 00 00 00 02      Body
                         00 01 00 00 00 00 00 02      Body, centred
54 45 58 54 0D 00 00 00  11 01 11 "Hi " 10 01 10 "you" 0D   TEXT: centred
45 4E 44 20 00 00 00 00                     END
```
