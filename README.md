# Veranda Write

A word processor for the Commander X16 under **Veranda** (the GEOS-API
desktop for the X16): several fonts and sizes on a page, bold, italic and
underline, many levels of undo and redo, documents much larger than the
X16's main memory, its own file format. A sibling of Veranda Calc.

This is **stage 2** of the plan below, with **geoWrite documents**
opened (converted). MIT licence, Copyright (c) 2026 Marcin Spoczynski.

It was called **Commander Write** until Veranda 0.9.0 (DECISIONS W40).
The repository keeps its name, `commander-write`; the program, its file
(`Veranda Write.cvt`) and its menus are Veranda Write. Its GEOS class is
still `CmdrWrite`, so documents made before open as they did.

## Features (stages 1 and 2)

- Text in **character styles**: any font in `/VERANDA/FONTS` (Gate Sans
  9, 12, 18, 27; Gate Mono 10, 12; the system font) at its sizes, **bold**,
  *italic*, underline, mixed freely on a line.
- **Word wrap** to a 6.5-inch column (A4 or Letter with 1-inch margins at
  80 pixels an inch), lines as tall as their tallest font.
- **Paragraph styles**: aligned left, centred, right or justified; left
  and first-line indents; space before and after; single, 1.5 or double
  line spacing. **Named styles** in the Style menu: Body, Heading 1, 2
  and 3 (27, 18 and 12 points, bold), Quote (indented both sides,
  italic), Bulleted list and Numbered list (the bullets and the numbers
  drawn by the layout, numbered by themselves). Return after a heading
  gives Body; Return in an empty list item ends the list. The toolbar
  shows and sets the alignment, the lists and the indent, and shows the
  paragraph's style.
- **Find and Replace**: a panel under the text (Find, Replace with, Match
  case, Whole word, Find next, Replace, Replace all); a match may span a
  style change; Replace all is one undo step.
- **Word count** (words, characters with and without spaces, paragraphs,
  pages; the selection's words), **Go to page**, **Insert date and
  time**.
- **Undo and redo**, many levels: typing, deleting, formatting, pasting.
- **Selection** with the mouse (click, Shift+click, drag, double click on
  a word) and the keyboard (Shift with any move).
- **Cut, copy, paste**: inside Veranda Write with the styles; with
  other programs through Veranda's clipboard (the Text Scrap), as text.
- **Documents larger than RAM**: up to 4 MB of text on any X16. The text
  and its line table are in RAM banks while there are banks, and on the
  card when there are not: a part not used lately is put out to a swap
  folder (`/VERANDA/.SYSTEM/CWSWAP.n`), or just dropped when the
  document's own file has it as it is, and read back when it is needed
  (DECISIONS W29). A big document shows its first page at once and is
  read on while you look at it ("Page 3 of ~120..."). The undo log stays
  in RAM.
- **Its own file format** (`.cwr`, versioned: `docs/FORMAT.md`; 1.1 adds
  the paragraph styles: 1.0's files open, all Body; a 1.0 reader shows a
  1.1 file's paragraphs as Body; 1.2 adds page breaks, which 1.1 shows as
  empty paragraphs); long file names; plain text files open too.
- **geoWrite documents open** (`Write Image V1.x` and `V2.x`, the `.cvt`
  files geoWrite wrote): the Open dialog lists them with the documents (by
  their class: other GEOS files are not listed), opening one asks
  "Convert this geoWrite file? The original is kept.", and it opens as
  *Untitled-name*; Save writes `name.cwr` (the geoWrite file is never
  written). Kept: the text, the fonts (their names and sizes; drawn in
  Gate Sans at the nearest size where the font is not on the card), bold,
  italic, underline (outline as bold), the rulers as paragraph styles
  (indents, alignment, line spacing), page breaks between paragraphs
  (drawn as a dashed line). Left out, and said so: the header and the
  footer, pictures (a `[picture]` line until stage 5), tab stops (every
  half inch here), superscript, subscript and reverse (plain), any byte
  that is not an ASCII character (as `?`). `tools/gwimport.py` does the
  same on a PC (`convert in.cvt out.cwr`; `info`). DECISIONS W24-W28,
  FORMAT.md "geoWrite documents".
- **Its own text drawing**: the fonts read from their files in
  `/VERANDA/FONTS`, rows put together in a RAM bank and written to both
  screens (italic drawn whole: Veranda's own text cuts its slant).
- **Alt+Tab** puts it aside with its document, as any Veranda program.
- Veranda's look (Look 3, Sunroom): the menu bar (Veranda Write, File,
  Edit, Text, Style, Format, Font, Size; their items in groups, each
  command's keys at the right), a title band with a close box, a toolbar
  (pop-up lists for the font, the size and the paragraph's style; the
  buttons B, I, U; the alignment, the lists, the indent), a ruler (the
  inches, the paragraph's indents), the page on the desk's colour, a
  scroll bar with arrows, a status bar (page, line, column, modified), a
  find panel in the same style. Light and dark, as Veranda's settings
  say.

## Keys

| Key | |
|---|---|
| Arrows | move (Shift: select); Ctrl+Left/Right: a word |
| Home, End | the line's start, end |
| PgUp, PgDn | a screen |
| Ctrl+Home, Ctrl+End (Ctrl+PgUp) | the document's start, end |
| Backspace, Delete | delete |
| Return, Tab | a new paragraph, a tab (every half inch) |
| Esc | no selection |
| Ctrl+N, O, S, Shift+S, W, Q | New, Open, Save, Save as, Close, Quit |
| Ctrl+Z, Y | Undo, Redo |
| Ctrl+X, C, V, A | Cut, Copy, Paste, Select all |
| Ctrl+T, B, I, U | Plain, Bold, Italic, Underline (the selection's, or what you type next) |
| Ctrl+E, Ctrl+Shift+E | a larger, a smaller size |
| Ctrl+L, Ctrl+Shift+C, Ctrl+Shift+R, Ctrl+J | align left, centre, align right, justify (the selection's paragraphs, or the cursor's) |
| Ctrl+F, Ctrl+H (Ctrl+R) | Find, Replace (the panel) |
| Ctrl+G | Find next (the panel open or not) |
| Ctrl+Shift+G | Go to page |
| Ctrl+Shift+D | Insert date and time |
| Alt + the same letters | the same |
| Alt+Tab | put Veranda Write aside (Veranda's switcher) |

In the find panel: typing goes to its field, Tab the other field,
Backspace, Return finds the next, Alt+C Match case, Alt+W Whole word,
Alt+N Find next, Alt+R Replace, Alt+A Replace all, Esc closes it; the
arrows and the other commands go to the document.

The mouse: a click places the cursor, Shift+click and a drag select, a
double click selects a word, the wheel scrolls (three lines a notch), the
scroll bar's arrows scroll three lines, its track above or below the
thumb a screen; the toolbar's pop-ups open their lists (the fonts, the
sizes, the paragraph styles: press and drag to an item, or click and
click), its buttons: B, I, U; left, centre, right, justify; a bulleted
list, a numbered list (again: Body); indent less, more; the title band's
close box closes the document. The **Style** menu has the named styles, **Format** the alignment,
the indent, the line spacing and the space after paragraphs, **Font**
the installed fonts, **Size** the sizes of the current font; Word count
is in the Veranda Write menu, Find, Replace and Go to page in Edit,
Insert date and time in Text.

## Limits (stage 2, documents larger than RAM)

- Text is ASCII and Veranda's set above it, ISO-8859-16: Polish (Ą Ć Ę
  Ł Ń Ó Ś Ź Ż, typed with right Alt and Veranda's Polish keyboard on),
  German, French, Hungarian, Romanian, Croatian (DECISIONS W38). A font
  draws them when it has them (Veranda's fonts since its V136); an older
  font draws `?`. Documents of 1.0-1.2 were Latin-1: they are converted
  when opened (the 40 letters the set has not become `?`, said in the
  status bar). A font is drawn from its file in `/VERANDA/FONTS`
  (one that is not there in Gate Sans); glyphs up to 48 pixels wide and
  fonts up to 48 rows.
- 128 character styles, 32 paragraph styles and 8 fonts in a document.
- **A document up to 4 MB of text and 65,000 lines** (16-bit line
  numbers; about 50,000 lines of ordinary text in 4 MB), with any amount
  of RAM: what does not fit is on the card, which needs room for it (at
  most the text and its line table, about 4.5 MB; DECISIONS W31). A
  bigger one is refused at Open. With 512 KB the cache is about 20 banks
  (160 KB); with 2 MB a 1 MB document stays in RAM whole.
- **Speed at 8 MHz** (the gate's 140,000-character document, 162 KB of
  text, at 512 KB): a key typed and drawn in about 0.26 s, 8 keys queued
  by fast typing and their drawing 0.34 s, Page Down 0.82 s (0.98 at
  most), its first screen 2.4 s after Open and all of it in 10.2 s
  (stage 2: 9.6 s before anything showed), a search through all of it
  1.7 s, Replace all of 624 words 26 s, saving 0.9 s.
- **A document larger than RAM** (an SD card image under R49, 512 KB):
  1 MB (980,000 characters, 12,000 lines): its first screen 3.3 s after
  Open, all of it read and wrapped in 75 s (only the line table's 7
  chunks written to the swap); Ctrl+End 0.66 s, Ctrl+Home 0.97 s (both
  from the card); Page Down 0.8-1.0 s; Go to page 2.8 s (all its pages
  counted); finding a word in its middle from the start 16 s (half of it
  read from the card: 8.8 s at 2 MB); a key typed in a chunk read back
  0.42 s; saving 30 s (18 s at 2 MB); Alt+Tab puts it aside (28 banks
  given to Veranda: all but the program's overlays, the undo log too
  out on the card) and brings it back. 4 MB (4.05 MB of text, 43,000
  lines): its first screen 3.3 s, all of it in 4 min 24 s; Ctrl+End
  0.64 s, Ctrl+Home 0.99 s, Page Down 0.8-1.0 s, Go to page 7.9 s,
  finding its middle from the start 56 s, a key typed there 0.50 s,
  saving 1 min 55 s; put aside and back twice.
  A chunk of 8 KB read from the card takes about 0.1 s, one written
  about 0.18 s (DECISIONS W29).
- While a document is being read, an edit near the end of what is in,
  undo, Save, Find, Replace all, Word count and Go to page wait for the
  rest (DECISIONS W30).
- The document's file is read again while it is open (the parts not in
  RAM): another program must not change it meanwhile (a second Veranda
  Write saving over it, while the first is put aside, would leave the
  first with the wrong text). Saving, as always, writes a new file and
  then replaces the old one; a crash leaves the document as last saved,
  and the swap it left is removed at the next start (DECISIONS W36).
- Veranda's Gate Mono is its test font (box glyphs): use Gate Sans.
- Undo keeps about 24 KB of changes (the oldest steps go first); a
  single change larger than that cannot be undone (it says so): Replace
  all of more than some 800 short words.
- The Text Scrap holds the first 60,000 characters of a copy (Veranda
  Write's own clipboard holds all of it).
- A numbered list counts its paragraphs from 1 for each run of numbered
  paragraphs; the pages are A4's 9.7 inches of text, nominal: a page break
  (from a geoWrite document) is drawn but does not start a page yet, and
  there is no command to make one (stage 3).
- The Open dialog lists the geoWrite documents of the folder it opens in
  (read before it opens); in a folder chosen inside the dialog they are
  not listed (W27). Converting one takes an overlay's RAM bank: the
  largest document at 512 KB is about 8 KB smaller than stage 2's.
- Pages, margins, headers and footers, printing: stage 3.

## Building

Veranda Write builds against Veranda's installed SDK, as Veranda's
package builds it (`tools/mkdist.sh`: `make veranda SDK=...`):

    tools/sdk.sh            # Veranda's SDK and fonts from ~/Projects/geos
                            # into build/sdk, build/fonts (not touching it)
    make test               # the document model's unit tests (gcc)
    make veranda            # build/veranda/Veranda Write.cvt
    tools/gate.sh           # the emulator gate (below)

You need cc65 (`cl65`, `CC65_HOME`), Python 3, x16emu and the X16 ROM
(`EMU`, `ROM`; R49 for the SD card run).

Put `Veranda Write.cvt` in `/VERANDA/APPS` on the card; the shell
opens `.CWR` documents with it.

## Tests

- `make test`: the document model on the host (`tests/test_main.c`):
  the chunked buffer against an array; 4000 random edits (typing in
  styles, typing runs, deleting, formatting, paragraph styles and the
  named styles, undo, redo) against a model of characters, styles and
  paragraphs, after each the line table checked against a fresh layout,
  each line's width and its text's place (the alignment) measured, the
  list numbers, the codes in their places; find against the model (case,
  whole words), Replace all against the model, undone and redone, no code
  left doing nothing; the counts; the file round trip, unknown chunks,
  1.0 and later minor versions, a newer major version, a damaged file,
  plain text (CR LF, LF, CR, UTF-8); a 200 KB document edited at both
  ends; **paging** (a simulated card, `tests/host.c`): the text's
  chunked buffer against an array with 5 banks free (every read and edit
  pages), appending from a document's file, the 200 KB document's session
  with 6 banks free (the same text and line table as with all the banks),
  a save (the old file gone: all of it read back from the new one), a
  save that fails, the card full (a big insertion refused whole);
  **opening a step at a time** with edits before the end of what is in;
  **the pages** (`src/v/pages.c`) against counting every line, through
  an opening and 300 edits; geoWrite documents made from the format's
  description (pages, rulers, fonts, styles, a page break, a picture, a
  header and a footer, odd bytes) converted and checked, saved and read
  back, a page break deleted and undone, a converted document of 500 KB
  with 6 banks free; a real one when `CW_GW_SAMPLE=path` names it. `build/host/test_cw SEED` runs another seed;
  `build/host/test_cw gw in.cvt out.cwr` converts a file.
- `tools/gate.sh`: the X16 build in x16emu with Veranda booted, a
  scripted session (typing in styles, the menus, a selection made bold,
  undo and redo, copy and paste, a double click, the wheel, Save as
  through the file chooser, Alt+Tab away and back, New, Open; a document
  with headings, lists, a quotation, centred and justified paragraphs,
  undone, redone, saved; Find, Replace all with match case, undone and
  redone, whole words; Word count; a 1.0 file opened; a geoWrite document
  (`tools/gwimport.py sample`) listed in the Open dialog, converted when
  asked (as `gwimport.py` converts it), saved as a `.cwr` (the original
  unchanged), opened again; a 140 KB document
  opened, edited, searched, replaced in (undone, redone) and saved, a
  burst of typing at full speed; a styled run at the end of a document
  deleted with Backspace held, Delete and Cut, W41), its states (characters, styles,
  paragraphs) and files checked against a model, the timings,
  screenshots (`build/gate/*.png`; the styled page, the find panel, Word
  count, the geoWrite document's steps also in `build/shots/`), on the host folder at 512 KB and on an
  SD card image under ROM R49 at 2 MB (a 200 KB document). Then **a 1 MB
  document on an SD card image, at 512 KB and at 2 MB** (`SESSION=big`:
  opened, Ctrl+End, Ctrl+Home, Page Down, Go to page, found in the
  middle, typed in, undone, redone, Alt+Tab, saved as BIG2.cwr, opened
  again; the states against the model, BIG2.cwr read back on the PC, the
  original BIG.cwr unchanged, a stale swap folder put on the card
  removed). `BIGDOC=3500000 BIGRAM=512` runs a 4 MB one (`BIG_NOSTATE=1`
  for the script: the test build's sum of 4 MB takes minutes).
  `tools/swapbench/run.sh` measures the card's speeds for the swap.
  `tools/check_ovl.py` checks that no code calls into an overlay that may
  not be mapped.

## The plan

1. **Text**: character styles, undo, selection, the clipboard, the file
   format, word wrap, large documents, Alt+Tab.
2. **Paragraphs** (this): paragraph styles (Heading 1-3, Body, Quote,
   lists, alignment, indents, spacing), find and replace, word count,
   its own text drawing.
3. Pages, margins, headers and footers, page numbers, footnotes,
   printing and PDF (Veranda's PrintOpen/PrintBand).
4. `.docx` and `.odt` import and export on the machine (Commander Calc's
   zip and deflate).
5. Tables, pictures from Paint, a spelling checker with a dictionary on
   the card.

The design: `DECISIONS.md` (W1, W2 ...). The file format:
`docs/FORMAT.md`.
