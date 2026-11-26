# Veranda Write's document format (CWRD)

Version 1.6 (1.0 plus paragraph styles: the `PARA` chunk and the `$11`
code, 1.1; the page break, the `$12` code, 1.2; the text's character set
in `INFO`, 1.3; the page setup, the `PAGE` chunk: the paper, the margins,
the header and the footer, 1.4; the spelling language in `INFO`, 1.5;
tables, the `$14`-`$16` codes, 1.6). A Veranda Write document is a plain file on the card (any
FAT name; the program adds `.cwr` when a name has no extension, and
Veranda's shell opens `.CWR` files with it). All numbers are little
endian. `src/format.c` reads and writes it on the X16 and in the host
tests; `tools/cwrd.py` does the same on a PC.

## The header (8 bytes)

| Offset | Size | Contents |
|---|---|---|
| 0 | 4 | `CWRD` |
| 4 | 1 | major version: 1 |
| 5 | 1 | minor version: 6 (1.0's files: 0, 1.1's: 1, 1.2's: 2, 1.3's: 3, 1.4's: 4, 1.5's: 5) |
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
| `INFO` | 1.0 | 4 bytes: the cursor (a place in the text); 2: the text's width in pixels at 80 an inch (520 until 1.3; in 1.4 the page setup's, as `PAGE` gives it: a reader takes `PAGE`'s); 1: the text's character set (1.3; reserved, 0, before): 0 Latin-1 (ISO-8859-1), 1 ISO-8859-16 (Veranda's set); 1: the spelling language (1.5; reserved, 0, before): 0 not said (the program's default: Polish when Veranda's keyboard setting is Polish, else English), 1 English, 2 Polish; a reader of 1.4 or before passes over it. Longer in later versions: read the first 8 bytes, skip the rest |
| `FONT` | 1.0 | 1 byte: n, then n names: a length byte and the name (Latin-1, at most 31 characters): the fonts the styles use, by the names Veranda's `LoadFont` takes ("Gate Sans", "System") |
| `STYL` | 1.0 | 1 byte: n (1-128 in 1.0), then n character styles of 4 bytes: the font (an index into `FONT`), the size in points, the flags, the colour (0; reserved for later) |
| `PARA` | 1.1 | 1 byte: n (1-32), then n paragraph styles of e bytes each, e = (the chunk's length - 1) / n: 8 in 1.1 (below); a later version may make them longer: read the first 8 of each. Style 0 is the style of every paragraph without a paragraph code. Must come before `TEXT`. Without it (a 1.0 file) there is style 0 only: Body |
| `PAGE` | 1.4 | the page setup: 142 bytes (below); a later version may make it longer: read the first 142. Must come before `TEXT`. Without it (a file of 1.0-1.3) the document has the default page setup of the program that opens it |
| `TEXT` | 1.0 | the text: the document's byte stream (below). Must come after `FONT`, `STYL`, `PARA` and `PAGE` |
| `END ` | 1.0 | length 0: the end |

Reserved for later versions: `NOTE` (footnotes), `OBJS` (pictures),
`META` (title, author, dates). (Tables, 1.6, are codes in the text: below.)

**The page setup** (`PAGE`, 1.4; sizes in twips, 1/1440 inch, 16 bits):

| Offset | Size | Contents |
|---|---|---|
| 0 | 1 | the paper: 0 A4 (11906 x 16838), 1 US Letter (12240 x 15840), 2 another (its size below) |
| 1 | 1 | flags: bit 0, no header and footer on the first page; bits 1-7 reserved (0) |
| 2 | 2 | the paper's width |
| 4 | 2 | its height |
| 6 | 2 | the top margin |
| 8 | 2 | the bottom margin |
| 10 | 2 | the left margin |
| 12 | 2 | the right margin |
| 14 | 64 | the header: its bytes (the text's set, `INFO`'s), ended by a 0 byte (63 at most) |
| 78 | 64 | the footer, the same |

The text's width is the paper's less the left and right margins, its
height on a page the paper's less the top and bottom ones (Veranda
Write: 80 pixels an inch, a pixel 18 twips; the text at least 3 inches
wide and high, at most 6.6 inches wide: the screen's page). The header
and the footer are in the top and bottom margins. A header or footer is
one line: `centre`, `left|right` or `left|centre|right` (a `|` past the
second is the text's own); `{page}`, `{pages}`, `{date}` and `{name}`
become the page's number, the number of pages, the date it is printed or
exported and the document's name (its file name without the extension);
any other `{` is itself. A reader that finds the paper's size or the
margins impossible uses A4 and margins of 2.5 cm (1 inch on Letter).

**The pages**: a page holds the lines that fit in the text's height; a
line that does not fit starts the next page (a line taller than a page
has one of its own), and the line after a page break's paragraph always
does (a page break's own line never moves to the next page for its
height). Veranda Write's view, its page numbers and its PDF all count
them so.

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
| `$12` | 1.2 | a page break: argument 0. A paragraph of its own: `[$12 0 $12]` and the paragraph's CR, nothing else in the paragraph (no paragraph code). The next paragraph starts a new page (1.4's programs; Veranda Write 1.2-1.3 drew a dashed line across the column there and went on). A page break in a paragraph with anything else in it does nothing (Veranda Write takes it out when a deletion next to it, Backspace or Delete, joins its paragraph to another; text typed into its paragraph leaves it there, doing nothing, and hides the line). A 1.1 reader keeps it and shows an empty paragraph |
| `$13` | (5) | an object: an index into `OBJS` (a picture) |
| `$14` | 1.6 | a table's row starts: the first paragraph of its first cell. Argument: flags, bit 0 no borders (the same in every row of a table), bits 1-7 reserved (0). Followed straight away by the first cell's `$15` |
| `$15` | 1.6 | a table's cell starts: its first paragraph. Argument: its width in 256ths of the text's width, 1-255 (0: 32) |
| `$16` | 1.6 | a further paragraph of the cell before it. Argument: 0 |
| `$17` | | reserved |

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

## Tables (1.6)

A table is paragraphs. Each paragraph of a table starts with a table
code, after its paragraph code (if it has one) and before anything else
but a character style code:

| A paragraph that is | starts with |
|---|---|
| a row's first cell's first paragraph | `[$14 f $14] [$15 w $15]` |
| another cell's first paragraph | `[$15 w $15]` |
| a cell's second, third ... paragraph | `[$16 0 $16]` |

A row is its `$14` paragraph and the `$15` and `$16` paragraphs after it,
up to the next paragraph that has no `$15` or `$16` (a new row, or text
after the table). A table is rows one after another; its rows may have
different numbers of cells (Veranda Write makes them alike). A row has
8 cells at most (a ninth cell's paragraphs are taken as the eighth's);
a `$16` paragraph with no cell before it, or a `$15` with no row, is
laid out as an ordinary paragraph. A cell's width is its share of the
text's width (256ths; the cells of a row should add up to 256 at most:
borders past the text's right edge are drawn at it). Each paragraph
of a cell keeps its own paragraph style (its alignment, a list) and
character styles. There are no tables inside tables and no merged
cells: a file from elsewhere has them as text (a cell's table) or as
one wider cell (merged ones).

On the screen and in a PDF, a row is as tall as its tallest cell and
never splits across pages; each cell's text wraps inside the cell, 4
pixels in from its left border and 3 from its right, its top 3 below
the row's top border; the borders are 1-pixel lines (none when bit 0
of `$14`'s flags is set). In Veranda Write a row is 255 pixels tall at
most (a taller cell's lines below that are kept, not drawn) and holds
2,000 bytes (typing stops there; a file's longer row is read).

A 1.5 (or older) reader keeps the codes and passes over them (they have
no width): it shows each cell's paragraphs as ordinary paragraphs, one
after another, and saves them back with the codes in place, so a 1.6
program opens the tables again.

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
43 57 52 44 01 01 00 00                     CWRD 1.1 (1.2 writes 01 02, 1.3 01 03, 1.4 01 04, 1.5 01 05)
49 4E 46 4F 08 00 00 00  00 00 00 00 08 02 00 00    INFO: cursor 0, width 520 (1.3: 08 02 01 00, ISO-8859-16; 1.5: 08 02 01 02, Polish spelling)
46 4F 4E 54 0B 00 00 00  01 09 "Gate Sans"          FONT: 1 font
53 54 59 4C 09 00 00 00  02 00 0C 00 00 00 0C 01 00 STYL: 2 styles (plain, bold)
50 41 52 41 11 00 00 00  02                         PARA: 2 paragraph styles:
                         00 00 00 00 00 00 00 02      Body
                         00 01 00 00 00 00 00 02      Body, centred
                                            (1.4: PAGE 8E 00 00 00 and its 142 bytes here)
54 45 58 54 0D 00 00 00  11 01 11 "Hi " 10 01 10 "you" 0D   TEXT: centred
45 4E 44 20 00 00 00 00                     END
```

## Older and newer programs

A 1.4 file opens in a program of 1.0-1.3 (a minor version): it skips
`PAGE`, so the document opens with that program's page (A4's 9.7 inches
of text, 6.5 inches wide), its page breaks shown as lines or empty
paragraphs; saving it there writes the older version without the page
setup. A file of 1.0-1.3 opens in 1.4 with the user's default page
setup (Veranda Write: File > Page setup, "For new documents"; A4 with 2.5
cm margins until one is chosen).

A 1.5 file opens in a program of 1.4 or before as it did: `INFO`'s last
byte was reserved and is passed over, so only the spelling language is
not kept (saved there, the file is the older version, language 0). A
file of 1.4 or before opens in 1.5 with the language not said: Veranda
Write checks it in its default language (Polish when Veranda's keyboard
setting is Polish, else English) and saves that one with it.
