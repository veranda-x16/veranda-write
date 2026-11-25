/* Veranda Write on Veranda: overlay 8 (with the font cache, fcache.c):
   the menus' lists made when they open (menus.c says how), the bar's
   layout, the Size menu's sizes (the toolbar: tbar.c).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#pragma code-name ("OVERLAY8")
#pragma rodata-name ("OVL8RO")
#pragma bss-name ("OVL8BSS")

/*---------------------------------------------------------------
  the menus
 ---------------------------------------------------------------*/
/* every list's items, the bar's in order (mfirst: where each starts):
   the name, the keys. A name that starts with "\x1f" starts a group (in
   the Veranda look a dotted line above it; Veranda's kernel draws the
   keys after a tab at the list's right) */
static const char *const names[][2] = {
    { "About Veranda Write", "" }, { "\x1fWord count...", "" },
    { "New", "Ctrl N" }, { "Open...", "Ctrl O" }, { "\x1fSave", "Ctrl S" }, { "Save as...", "Ctrl Shift S" },
    { "\x1fPage setup...", "" }, { "Export as PDF...", "Ctrl Shift P" }, { "Export as Word (.docx)...", "" },
    { "Export as OpenDocument (.odt)...", "" },
    { "\x1f" "Close", "Ctrl W" }, { "Quit", "Ctrl Q" },
    { "Undo", "Ctrl Z" }, { "Redo", "Ctrl Y" }, { "\x1f" "Cut", "Ctrl X" }, { "Copy", "Ctrl C" },
    { "Paste", "Ctrl V" }, { "Select all", "Ctrl A" }, { "\x1f" "Find...", "Ctrl F" }, { "Find next", "Ctrl G" },
    { "Replace...", "Ctrl H" }, { "Go to page...", "Ctrl Shift G" },
    { "Page break", "Ctrl Return" }, { "Date and time", "Ctrl Shift D" }, { "\x1fHeader and footer...", "" },
    { "Plain", "Ctrl T" }, { "Bold", "Ctrl B" }, { "Italic", "Ctrl I" }, { "Underline", "Ctrl U" },
    { "\x1fLarger", "Ctrl E" }, { "Smaller", "Ctrl Shift E" },
    { "Body", "" }, { "\x1fHeading 1", "" }, { "Heading 2", "" }, { "Heading 3", "" }, { "\x1fQuote", "" },
    { "\x1f" "Bulleted list", "" }, { "Numbered list", "" },
    { "Align left", "Ctrl L" }, { "Centre", "Ctrl Shift C" }, { "Align right", "Ctrl Shift R" },
    { "Justify", "Ctrl J" }, { "\x1fIndent more", "" }, { "Indent less", "" }, { "\x1fSingle spacing", "" },
    { "One and a half lines", "" }, { "Double spacing", "" }, { "\x1fSpace after paragraphs", "" },
};

/* s's width in the font now: a group's mark not drawn, a tab 24 pixels
   (the gap before the keys) */
unsigned int __fastcall__ strw(const char *s)
{
    unsigned int w = 0;
    for (; *s; ++s)
        if (*s == '\t')
            w += 24;
        else if (*s != '\x1f')
            w += GetCharWidth(*s);
    return w;
}

static void put_u(char *p, unsigned char v)
{
    if (v >= 10)
        *p++ = (char)('0' + v / 10);
    *p++ = (char)('0' + v % 10);
    strcpy(p, " pt");
}

void menu_make(void)
{
    unsigned char b = menu_bar, i, n, a = 0;
    unsigned int mx = 0;
    char *p;
    UseFont(FONT_UI);                   /* (the menus' font: theme_menus, Veranda's V79) */
    if (b == 7) {
        n = fm_n;
        for (i = 0; i < n; ++i)
            strcpy(itext[i], fm_names[i]);
    } else if (b == 8) {
        menu_choice = ty_style;
        sizes_fill();
        n = sz_n;
        for (i = 0; i < n; ++i)
            put_u(itext[i], sz_list[i]);
    } else {
        a = mfirst[b];
        n = mfirst[b + 1] - a;
        for (i = 0; i < n; ++i) {
            p = itext[i];
            strcpy(p, names[a + i][0]);
            if (names[a + i][1][0]) {
                strcat(p, "\t");
                strcat(p, names[a + i][1]);
            }
        }
    }
    for (i = 0; i < n; ++i) {
        msub.i[i].text = itext[i];
        msub.i[i].type = MENU_ACTION;
        msub.i[i].p = (void *)m_item;
        if (strw(itext[i]) > mx)
            mx = strw(itext[i]);
    }
    msub.top = 14;
    msub.bottom = 14 + 14 * n;
    msub.left = 0;
    msub.right = mx + 16;               /* (the list: 8 more; the text 8 in, the keys 8 from its right) */
    msub.flags = VERTICAL | n;
    UseSystemFont();
}

void menus_layout(void)
{
    unsigned int x = 0;
    unsigned char i, n;
    UseFont(FONT_UI);
    /* the fonts: FontList's, but the UI font (not one for documents) */
    n = FontList(iobuf, 10);            /* (iobuf is free now) */
    fm_n = 0;
    for (i = 0; i < n && fm_n < 8; ++i)
        if (strcmp((char *)iobuf + i * FONT_LEN, "Veranda UI"))
            strcpy(fm_names[fm_n++], (char *)iobuf + i * FONT_LEN);
    /* the bar as the kernel lays it out in the Veranda look (its
       kernel/u/menu.s, the SDK's vr_bar_end): the mark, item 0 (bold) 24
       pixels in, 6 pixels each side of a text, each item to a whole card */
    for (i = 0; i < NBAR; ++i) {
        barx[i] = x + (i ? 6 : 30);
        x += (i ? 12 : 30) + strw(mbar.i[i].text) + (i ? 0 : strlen(mbar.i[i].text));
        x = (x + 7) & ~7;
    }
    mbar.right = x + 64;
    UseSystemFont();
}

void sizes_fill(void)
{
    unsigned char i;
#pragma bss-name (push, "BSS")
    static char nm[FONT_LEN];           /* (main RAM: the kernel reads it) */
#pragma bss-name (pop)
    strcpy(nm, fontname[sty[menu_choice].font]);    /* (below $A000, for the kernel) */
    memset(iobuf, 0, 64);
    sz_n = 0;
    FontSizes(nm, iobuf);
    for (i = 0; i < 64 && iobuf[i] && sz_n < 10; ++i)
        sz_list[sz_n++] = iobuf[i];
    if (!sz_n) {
        sz_list[0] = 9;                 /* (no such font: the system font's) */
        sz_n = 1;
    }
}
