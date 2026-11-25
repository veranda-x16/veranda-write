# Veranda Write

A word processor for the Commander X16 under **Veranda** (the GEOS-API
desktop for the X16): several fonts and sizes on a page, bold, italic and
underline, many levels of undo and redo, documents much larger than the
X16's main memory, its own file format. A sibling of Veranda Calc.

This is **stage 4** of the plan below (Word and OpenDocument documents
opened and exported), with stage 3's pages, headers and footers and PDF,
and **geoWrite documents** opened (converted). MIT licence, Copyright (c)
2026 Marcin Spoczynski.

It was called **Commander Write** until Veranda 0.9.0 (DECISIONS W40).
The repository keeps its name, `commander-write`; the program, its file
(`Veranda Write.cvt`) and its menus are Veranda Write. Its GEOS class is
still `CmdrWrite`, so documents made before open as they did.

## Features (stages 1 to 4)

- **Spelling, English and Polish** (W56-W59; docs/SPELLING.md): words
  the dictionary has not are underlined with a dotted line as you write
  (looked at when typing pauses, so typing is as quick as without it) and
  on every screen of a document; **Tools > Check spelling** (F7) goes
  through the document from the cursor in a panel under the text: the
  word in its context, up to six suggestions ("ksiazka" → "książka",
  "zolw" → "żółw", "recieve" → "receive"), Change, Change all, Ignore,
  Ignore all, Add (to your own words), Close; a right click on a marked
  word (or F8) opens its suggestions under it. The document's language
  is kept in it (Tools > English, Polish; a new one is English until
  Tools > Spelling options says otherwise). The dictionaries are Writer's
  Polish one (300,000 word forms) and an English one from SCOWL (165,000),
  in `/VERANDA/DICT`; your own words in `/VERANDA/.SYSTEM/WRITE.UDIC`.
  The dictionaries are a separate download (1.9 MB, W61): **Tools > Get
  dictionaries** has Veranda Update fetch them over the Wi-Fi modem, or
  copy them from `Veranda-Write-dictionaries.zip` (this repository's
  release `dict-1`, made by `make dict-release`).
  **Tools > Spelling options** (W60) keeps three settings between runs,
  in `/VERANDA/.SYSTEM/WRITE.CFG`: check spelling as you work (off: only
  when you ask with F7, F8 or the Tools menu; nothing is loaded or marked
  before), mark misspellings as you type (Tools > Mark misspellings
  changes it too and keeps it at once), and the language of new
  documents and of those saved without one (English, the default;
  Polish; or the keyboard's: Polish when Veranda's keyboard setting is).

- **Word and OpenDocument documents open** (stage 4): File > Open lists
  `.docx` and `.odt` files with the documents and converts one when it is
  opened (the original is never written): it opens as *Untitled-name*,
  Save as offers `name.cwr`, "Converted from Word" (or OpenDocument) in
  the status bar and what was changed in a box. Kept: the text (Polish
  and the rest of Veranda's set; a character it has not becomes `?`,
  counted), paragraphs with their alignment, indents, spacing and line
  spacing, Heading 1-3 (by the style's name in any language or its
  outline level), quotations, bulleted and numbered lists, bold, italic,
  underline, sizes (made Write's: 9, 12, 18, 27; Gate Mono for a
  monospaced font, Gate Sans for the others), tabs, line breaks, page
  breaks (and a paragraph's "page break before"), the paper and margins,
  the header and the footer (one line each, with page numbers as
  {page} and {pages}, "not on the first page"). Tables become their
  text, a row a paragraph, its cells apart by tabs. Left out, and said:
  pictures, drawings, formulas, text boxes, footnotes and endnotes,
  comments, the deleted text of tracked changes (the inserted text is
  kept: the changes accepted). A document of several hundred kilobytes
  opens with 512 KB: nothing of the file is kept whole, it is read and
  unpacked a little at a time.
- **Export as Word and as OpenDocument** (stage 4, File > Export as Word
  (.docx), Export as OpenDocument (.odt)): beside Export as PDF, by
  default in `/DOCUMENTS` with the document's name; the document itself
  stays a `.cwr`. The file holds the paragraphs with their styles, the
  headings, the lists, the runs' fonts (Arial, Courier New for Gate Mono),
  sizes, bold, italic and underline, tabs, page breaks, the page setup,
  the header and the footer with their fields (page, pages, date, name),
  Polish letters in UTF-8; Word and LibreOffice open it. It is written as
  it goes; the status bar shows how far, Esc stops it.

- **Pages** (stage 3): **File > Page setup** chooses the paper (A4 or US
  Letter) and the margins (in centimetres on A4, inches on Letter), kept
  in the document (`.cwr` 1.4); "For new documents" makes it the default
  for new ones and for documents made before 1.4 (A4 with 2.5 cm margins
  until then). The text wraps to the paper's width less the margins and
  fills the paper's height less them: a line that does not fit starts
  the next page. **Insert > Page break** (Ctrl+Return) ends a page where
  you want; it shows as a dotted line labelled "Page break". The view
  shows where each page ends: the page's footer, a grey band, the next
  page's header. The status bar says "Page 3 of 12"; Word count and Go to
  page count the same pages.
- **A header and a footer** (Insert > Header and footer): one line each,
  "centre", "left|right" or "left|centre|right", with the fields
  {page}, {pages}, {date} and {name}; "Not on the first page"; in the
  top and bottom margins, shown in the view between the pages.
- **Export as PDF** (File > Export as PDF, Ctrl+Shift+P): a PDF 1.4 file
  of the document, by default in `/DOCUMENTS` with the document's name:
  its pages at the paper's size and margins, with the header and footer,
  bold, italic and underline, the paragraph styles' sizes, alignment and
  indents, lists, page breaks; the text in PDF's own fonts (Helvetica,
  Courier for Gate Mono), selectable and searchable, Polish letters too.
  It is written as it goes, a little at a time (a document of megabytes
  needs no more memory than a page); the status bar shows the page, Esc
  stops it.
- **Printing**: the X16 has no standard printer, so Veranda Write prints
  through PDF: export the document, copy the PDF to a PC (or open it from
  the card there) and print it from any PDF viewer.
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
| Ctrl+Return | Insert a page break |
| Ctrl+Shift+P | Export as PDF |
| Alt + the same letters | the same |
| F7 | Check spelling (the panel; F7 again: Ignore) |
| F8, a right click on a word | its spelling suggestions (a pop-up: a click or 1-7, Esc; the right click only while spelling is checked as you work) |
| Alt+Tab | put Veranda Write aside (Veranda's switcher) |

In the page setup panel (File > Page setup, Insert > Header and footer):
Tab and the up and down arrows move between its items, left and right
(and Space) change the paper, a margin (a millimetre or a twentieth of
an inch a step) or an option, typing edits the header or the footer,
Return is OK, Esc Cancel; the pointer clicks its buttons, the margins'
- and +, the fields and the options.

In the spelling options (Tools > Spelling options): Tab and the up and
down arrows move between its three items, Space and left and right
change one, Return is OK (the settings kept), Esc Cancel; the pointer
clicks the boxes, the languages, OK and Cancel.

In the spelling panel: typing edits "Change to", Up and Down put the
suggestions in it, Return changes the word (Alt+C), Alt+L Change all,
Alt+I Ignore, Alt+G Ignore all, Alt+A Add, Alt+1 to Alt+6 a suggestion,
Esc closes it; a click on a suggestion puts it in "Change to", a double
click changes the word with it.

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
close box closes the document. **Insert** has a page break, the date and time, the header and
footer. The **Style** menu has the named styles, **Format** the alignment,
the indent, the line spacing and the space after paragraphs, **Font**
the installed fonts, **Size** the sizes of the current font; Word count
is in the Veranda Write menu, Find, Replace and Go to page in Edit,
Page setup, Export as PDF, Export as Word and Export as OpenDocument in
File.

## Limits (stage 4, documents larger than RAM)

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
- **Spelling** (W56-W59, docs/SPELLING.md): two languages, English and
  Polish, a document in one of them (a Polish quotation in an English
  document is marked). A word is letters (an apostrophe between them
  too); a hyphenated word is checked as its parts; words with digits, of
  one letter, or in capitals of up to five letters are not checked.
  Suggestions are not ordered by how common they are ("wich": "rich"
  before "which"), take up to 2.5 s at 8 MHz, and are at most eight. The
  user's words: about 3,000 bytes (some 300 words), the ignored ones
  about 2,000 (for the session). The marks of the line being typed
  disappear while you type and come back when you pause. At 8 MHz and
  512 KB (DECISIONS W59): a word met before 0.4 ms, one from the cache
  2.5-3 ms, from the card 5-6 ms (a host folder) or about 45 ms (an SD
  card image); Check spelling through 2,000 words 4-6 s the first time
  and 1.5-2.3 s again (a host folder), 14-21 s and 2.5-5 s (an SD card
  image); its suggestions 0.3-1.7 s, at most 2.5 s. Spelling's code
  (`SPELL.OVL`) and banks are given back before a document is opened
  or exported and read again when spelling is next wanted.
- **A document up to 4 MB of text and 65,000 lines** (16-bit line
  numbers; about 50,000 lines of ordinary text in 4 MB), with any amount
  of RAM: what does not fit is on the card, which needs room for it (at
  most the text and its line table, about 4.5 MB; DECISIONS W31). A
  bigger one is refused at Open. With 512 KB the cache is about 20 banks
  (160 KB); with 2 MB a 1 MB document stays in RAM whole.
- **Speed at 8 MHz** (the gate's 140,000-character document, 162 KB of
  text, at 512 KB): Page Down 0.82 s (0.98 at
  most), its first screen 2.4 s after Open and all of it in 10.2 s
  (stage 2: 9.6 s before anything showed), a search through all of it
  1.7 s, Replace all of 624 words 26 s, saving 0.9 s.
- **Typing at 8 MHz** (512 KB; from the key to its character and the
  cursor on the screen, the status bar after them; DECISIONS W43, W45): at a
  document's end 0.05 s, in a paragraph of 16 lines 0.1 s, in italic or
  bold 0.12-0.13 s, Backspace 0.12 s, Return 0.19 s, in a list of
  one-line paragraphs 0.08 s, in a 400 KB document 0.04-0.11 s. Keys
  typed faster than that are taken together and drawn once (at least 5
  times a second): 30 keys a second, none lost.
- **A document larger than RAM** (an SD card image under R49, 512 KB):
  1 MB (980,000 characters, 12,000 lines): its first screen 3.3 s after
  Open, all of it read and wrapped in 75 s (only the line table's 7
  chunks written to the swap); Ctrl+End 0.66 s, Ctrl+Home 0.97 s (both
  from the card); Page Down 0.8-1.0 s; Go to page 2.8 s (all its pages
  counted); finding a word in its middle from the start 16 s (half of it
  read from the card: 8.8 s at 2 MB); a key typed in a chunk read back
  0.42 s; saving 30 s (18 s at 2 MB); Alt+Tab puts it aside (25 banks
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
  paragraphs.
- **Pages**: A4 and US Letter (a document from elsewhere with another
  paper keeps it); the text 3 to 6.6 inches wide (the screen's page: on
  Letter the left and right margins together are 1.9 inches at least, on
  A4 4.3 cm) and 3 inches high at least. One header and one footer, a
  line of 63 characters each, in a small font in the view (Helvetica 9 in
  the PDF). No widow and orphan control, no footnotes, no different
  headers for odd and even pages. Page setup is not an undo step.
- **The PDF** uses PDF's standard fonts (nothing embedded: the file is
  small, and any PDF viewer has them): Helvetica for Gate Sans and the
  other proportional fonts, Courier for Gate Mono, in their bold and
  oblique forms. The lines and pages are the ones the screen has (Veranda
  Write's own fonts measure them): a line is placed where the screen has
  it and set in the PDF font from its paragraph's edge; a line the PDF
  font makes wider than its paragraph is narrowed a little to fit.
  Characters outside the Veranda set do not occur; tabs are every half
  inch. At 8 MHz a PDF takes up to a second a page: a 350 KB document of
  80 pages, its style changing every few words, in about a minute
  (820 KB of PDF).
- **Word and OpenDocument**: what a file has beyond the features above is
  left out (pictures, footnotes, comments ...: the note says which) or
  made Write's: fonts other than monospaced ones become Gate Sans, sizes
  the nearest of Write's, colours and tab stops are not kept (Write's
  tabs are every half inch), line spacing is single, 1.5 or double, the
  header and footer are the last section's (a `.docx`) or the document's
  master page's (an `.odt`), not the first page's or even pages' own. A
  file with more than 32 paragraph styles or 128 character styles gets
  the nearest it can. Exported, a font other than Gate Sans and Gate Mono
  is Arial (read back: Gate Sans); in an `.odt` two page breaks in a row
  have an empty paragraph between them. The files are not compressed
  (Word and LibreOffice read them; they are some times larger than
  theirs). At 8 MHz a `.docx` of 600 KB of XML (about 100 KB of text)
  opens in about a minute; exporting it takes about as long
  (DECISIONS W54).
- The Open dialog lists the geoWrite documents of the folder it opens in
  (read before it opens); in a folder chosen inside the dialog they are
  not listed (W27). Converting one takes an overlay's RAM bank: the
  largest document at 512 KB is about 8 KB smaller than stage 2's.

## Credits

- The spelling: its dictionary format, lookup, word lists and
  suggestions are taken over from **Writer**, the author's own earlier
  word processor for the X16; so is `PL.DIC`, Writer's Polish
  dictionary, built from **Morfologik 2.1** by Marcin Miłkowski (BSD
  2-clause) and the **FrequencyWords** Polish list by Hermit Dave (CC
  BY-SA 4.0): `dict/PL.DIC.txt`. `EN.DIC` is built from **SCOWL**
  2020.12.07 by Kevin Atkinson and contributors (a permissive licence):
  `dict/EN.DIC.txt`. Each licence file travels with its dictionary (the
  release `dict-1` has both, and so has its zip).

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
opens `.CWR` documents with it. Spelling's code, `SPELL.OVL` (`make
veranda` puts it in `build/veranda/DICT`), goes in `/VERANDA/DICT`, from
the same build as the program; the dictionaries (`dict/`: `PL.DIC`,
`EN.DIC` and their licences) go there too, by hand or with Tools > Get
dictionaries (docs/SPELLING.md). `make dict-release` makes their GitHub
release's files in `build/dict-release` and prints the `gh` commands
that publish them (only you run those; or Actions > Dictionaries).
`tools/fetch_scowl.sh` rebuilds `EN.DIC` from SCOWL.

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
  with 6 banks free; a real one when `CW_GW_SAMPLE=path` names it;
  **stage 3**: the pages with page breaks in them (typed, typed into,
  deleted by the edits) against counting every line, the view's page
  starts; the header's and footer's fields and parts; the page setup's
  defaults, its checks, `PAGE` saved and read back, a 1.3 file (the
  default), a later version's longer `PAGE`; the PDF of a document of
  every style and paragraph style (Polish in it, a header and a footer,
  not on the first page), on Letter, stopped after two pages, of an empty
  document, of a 300 KB document with page breaks: its pages the pages'
  (each page's first line), its cross-reference table and startxref, then
  (`tools/check_pdf.py`) its structure again, and with qpdf, pdfinfo and
  pdftotext (when they are there) its pages, paper, header, footer and
  text, Polish too. `build/host/test_cw SEED` runs another seed;
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
  **Stage 3** (`SESSION=page`, ROM R49, 512 KB, a host folder and an SD
  card image): page breaks from the menu and Ctrl+Return, the header and
  footer typed in the panel, Letter clicked and the top margin stepped,
  saved (1.4: `PAGE` read back on the PC) and opened again, exported as
  PDF into `/DOCUMENTS` (its pages, paper, header and footer, not on the
  first page, the typed text, Polish), documents of 1.1, 1.2 (a page
  break) and 1.3 opened, a 300 KB document's pages counted and exported
  (timed), an export stopped with Esc (its file gone), Go to page 5 (the
  cursor where the PDF's page 5 starts); screenshots in `build/gate`
  (`pbreak`, `hfpanel`, `bands`, `setup`, `pdfdlg`, `old`, `goto`,
  `bigpages`) and `build/shots/`.
  `tools/swapbench/run.sh` measures the card's speeds for the swap.
  `tools/speed.sh` times each key typed (the gate runs it: DECISIONS
  W43). The gate's `SESSION=redraw` (R49, 512 KB) checks the drawing:
  after each change of the layout (paragraph and character styles,
  sizes, fonts, alignment, indents, spacing, lists, the page setup, undo
  and redo; with and without a selection, at the end, in the middle,
  scrolled off the top, under a footer with {pages}) the text area as
  the edits drew it is compared with the text wrapped again and drawn
  whole (DECISIONS W55); screenshots `rdhead`, `rdbug`, `rdtop1`,
  `rdend`.
  `tools/check_ovl.py` checks that no code calls into an overlay that may
  not be mapped.
- **Stage 4** (`make test`): Word and OpenDocument files written as Word
  and LibreOffice write them (`tools/office_fix.py`: every feature,
  Polish, a table, pictures, a text box, a footnote, a comment, tracked
  changes, fields, a header and a footer; 300 KB of XML) converted
  (`test_cw ox in.docx out.cwr`, the program's own code) and checked
  (`tools/check_office.py`), and, when LibreOffice is installed, the same
  files saved by LibreOffice in the other format and converted again; a
  document of every style exported as `.docx` and `.odt` and read back
  (the same text, styles, paragraphs, page breaks and page setup), a
  300 KB one read back with 10 banks free; each exported file checked by
  `tools/check_docx.py` (its ZIP, its XML, and the words LibreOffice reads
  in it). The gate's `SESSION=office` (R49, 512 KB, a host folder and an
  SD card image): a `.docx` and an `.odt` opened (their documents as the
  host's conversion of the same files), the note, exported both ways
  into `/DOCUMENTS` (read back on the host: the document the program had;
  `tools/check_docx.py`), a big `.docx` opened and exported (timed);
  screenshots `oxnote`, `oxdoc`, `oxsave`, `oxodt` in `build/shots/`.
- **Spelling** (`make test`, tests/test_spell.c): words with every Polish
  letter in lower case, capitalised and in capitals, English
  contractions and possessives, the text's words (hyphens, digits,
  abbreviations, apostrophes), the suggestions ("ksiazka" → "książka",
  "zolw" → "żółw", "lodz" → "łódź", "recieve" → "receive", "teh" →
  "the", "dont" → "don't"), the user's words (their UTF-8 file) and the
  ignored ones, 20,000 lookups a language against the dictionary's own
  words (with a cache of one bank and of four), a 2,000-word text checked
  twice (the second time from the hash). The gate's `SESSION=spell` (R49,
  512 KB, a host folder and an SD card image): live marks, Check spelling
  through a Polish and an English document (Change, Change all, Ignore,
  Ignore all, Add), the pop-up by a right click and by F8, the Tools menu,
  Alt+Tab, the files; the lookups and Check spelling over 2,000 words
  timed; (W60) the settings changed from the Tools menu and the options,
  the program quit and started again (the session's second part), the
  settings as they were, no marks and spelling not loaded while typing,
  Check spelling in the settings' language, `WRITE.CFG`'s bytes;
  `make test` reads and writes `WRITE.CFG` (missing, short, other and
  later files); screenshots `splivepl`, `spdlg`, `spdone`, `spliveen`,
  `sppop`, `splang`, `spopts`, `spnomark` in `build/shots/`. `SESSION=redraw` and the per-key timings
  run with the dictionaries on the card.

## The plan

1. **Text**: character styles, undo, selection, the clipboard, the file
   format, word wrap, large documents, Alt+Tab.
2. **Paragraphs**: paragraph styles (Heading 1-3, Body, Quote,
   lists, alignment, indents, spacing), find and replace, word count,
   its own text drawing.
3. **Pages**: the page setup, page breaks, headers and footers with page
   numbers, PDF export (the way to print). Footnotes: later.
4. **`.docx` and `.odt`** (this): opened (converted) and exported on the
   machine (Commander Calc's ZIP, inflate and XML reader, reworked to
   stream).
5. Tables, pictures from Paint, a spelling checker with a dictionary on
   the card.

The design: `DECISIONS.md` (W1, W2 ...). The file format:
`docs/FORMAT.md`.
