/* Veranda Write on Veranda: the page setup panel and File > Export as
   PDF (overlay 16; DECISIONS W44, W46, W47).

   The page setup panel (File > Page setup, Insert > Header and footer):
   under the text, as the find panel is (the view's bottom ta_bot moves
   up to PP_Y - 1): the paper (A4, Letter), the four margins (in
   centimetres on A4, inches on Letter; the arrows or the - and + boxes
   step them by a millimetre or a twentieth of an inch), the header and
   the footer (one line each: "centre", "left|right", "left|centre|right";
   the fields {page} {pages} {date} {name}), "Not on the first page",
   "For new documents" (the setup becomes the default, kept on the card
   in /VERANDA/.SYSTEM/CWPAGE.DEF), OK and Cancel. While it is open every
   key and click is its own: Tab and the up and down arrows move between
   its items, left and right (and Space) change one, typing edits a
   field, Return is OK, Esc Cancel. OK keeps the setup in the document
   (not an undo step: it marks the document modified) and wraps the text
   again when the paper or the margins changed.

   Export as PDF: the Save dialog (the SDK's) opens in /DOCUMENTS with the
   document's name and ".pdf"; a file there is replaced when the user
   says so. All of a document being opened is read first, its pages
   counted (for {pages}), then pdf.c (overlay 15) writes the file a
   buffer at a time, the status bar saying which page, Esc stopping it
   (the file is then deleted). The document's folder is the program's
   again after.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrchoose.h>
#include <vrpath.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY16")
#pragma rodata-name ("OVL16RO")
#pragma bss-name ("OVL16BSS")

extern char run[100];                   /* (view.c: main RAM, what the kernel draws) */

extern char pdf_name[64];               /* (main.c: the dialog and the kernel read it) */
#define name pdf_name

static void __fastcall__ text(const char *s, unsigned int x, unsigned int y)
{
    strncpy(run, s, sizeof run - 1);
    run[sizeof run - 1] = 0;
    WPutString(run, x, y);
}

static unsigned int __fastcall__ width(const char *s)
{
    unsigned int w = 0;
    while (*s)
        w += GetCharWidth(*s++);
    return w;
}

/*---------------------------------------------------------------
  the default for new documents (pgview.c reads it at the start)
 ---------------------------------------------------------------*/
static char defname[] = "CWPAGE.DEF";

static void savedef(void)
{
    unsigned char h, e = 1;
    vr_cd(VR_DATA);
    h = FOpen(defname, 'w');
    if (h >= 2) {
        e = FWrite(h, &pgs, PGS_N);
        e |= FClose(h);
    }
    vr_cdback();
    if (e)
        message("The default could not be written", "(is the card full?)");
    callb(bank14, pg_loaddef);          /* (overlay 14's pgs_def: read again) */
}

/*---------------------------------------------------------------
  the panel
 ---------------------------------------------------------------*/
#define PP_Y  304                       /* the panel: card rows 38-57 */
#define Y0    (PP_Y + 8)                /* the paper, Top, Bottom */
#define Y1    (PP_Y + 32)               /* Left, Right */
#define Y2    (PP_Y + 56)               /* the header */
#define Y3    (PP_Y + 80)               /* the footer */
#define Y4    (PP_Y + 104)              /* the options, OK, Cancel */
#define YH    (PP_Y + 132)              /* the explanation's baselines */
#define YM    (PP_Y + 156)              /* the message's */
#define LX    (PG_L + 10)               /* the labels */
#define FX    112                       /* the controls */
#define FW    472
#define MX0   288                       /* the margins: two columns */
#define MX1   432
#define OKX   456
#define CNX   520
#define NIT   9                         /* the items: the paper, the margins (1-4), the header, */
                                        /* the footer, the two options */

static struct pgset bak;                /* (Cancel: the setup as it was) */
static unsigned char foc, def_on;

static const char *const lab[NIT] = { "Paper", "Top", "Bottom", "Left", "Right", "Header", "Footer",
                                      "Not on the first page", "For new documents" };
static const unsigned int lx[NIT] = { LX, MX0, MX1, MX0, MX1, LX, LX, FX + 14, MX0 + 14 };
static const unsigned int ly[NIT] = { Y0, Y0, Y0, Y1, Y1, Y2, Y3, Y4, Y4 };

static unsigned short *__fastcall__ mg(unsigned char k)
{
    return k == 1 ? &pgs.top : k == 2 ? &pgs.bottom : k == 3 ? &pgs.left : &pgs.right;
}

static void ui(void)
{
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    UseFont(FONT_UI);
    currentMode = 0;
    SetPattern(0);
}

/* a box w pixels wide at x on row y (whole cards, white), s centred in
   it; on: lit */
static unsigned int by;

static void __fastcall__ button(unsigned int x, unsigned char w, const char *s, unsigned char on)
{
    ColorRect(x >> 3, w >> 3, by >> 3, 2, 0);
    WRectangle(x, by, x + w - 1, by + 15);
    text(s, x + ((w - width(s)) >> 1), by + 11);
    rbox(x + 1, by + 1, x + w - 2, by + 14);
    if (on)
        WInvertRectangle(x + 2, by + 2, x + w - 3, by + 13);
}

static char *__fastcall__ num(char *p, unsigned int v)
{
    char b[6];
    unsigned char n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *p++ = b[--n];
    return p;
}

/* a margin's value: "2.5 cm" (A4: millimetres), "1.00 in" (Letter) */
static void __fastcall__ value(char *p, unsigned short tw)
{
    static unsigned int v;
    if (pgs.paper == PP_LETTER) {
        v = (unsigned int)(((unsigned long)tw * 100 + 720) / 1440);
        p = num(p, v / 100);
        *p++ = '.';
        *p++ = (char)('0' + v / 10 % 10);
        *p++ = (char)('0' + v % 10);
        strcpy(p, " in");
    } else {
        v = (unsigned int)(((unsigned long)tw * 254 + 7200) / 14400);
        p = num(p, v / 10);
        *p++ = '.';
        *p++ = (char)('0' + v % 10);
        strcpy(p, " cm");
    }
}

/* item k drawn (its label lit when it has the focus) */
static void __fastcall__ item(unsigned char k)
{
    static char v[12];
    static unsigned int x, w;
    static const char *t, *e;
    ui();
    by = ly[k];
    x = lx[k];
    if (k == 0) {
        button(FX, 40, "A4", pgs.paper == PP_A4);
        button(FX + 48, 56, "Letter", pgs.paper == PP_LETTER);
        WRectangle(FX, Y1, MX0 - 8, Y1 + 15);
        text(pgs.paper == PP_LETTER ? "(inches)" : "(centimetres)", FX, Y1 + 11);
        for (k = 1; k < 5; ++k)
            item(k);
        k = 0;
        ui();
        by = Y0;
        x = LX;
    } else if (k < 5) {
        button(x + 48, 16, "-", 0);
        button(x + 112, 16, "+", 0);
        ColorRect((x + 64) >> 3, 6, by >> 3, 2, 0);
        WRectangle(x + 64, by, x + 111, by + 15);
        value(v, *mg(k));
        text(v, x + 66, by + 11);
    } else if (k < 7) {
        /* a field: its frame, its text (a long one's end), the cursor */
        t = k == 5 ? pgs.head : pgs.foot;
        ColorRect(FX >> 3, FW >> 3, by >> 3, 2, 0);
        WRectangle(FX, by, FX + FW - 1, by + 15);
        for (w = 0, e = t; *e; ++e)
            w += GetCharWidth(*e);
        while (w > FW - 14)
            w -= GetCharWidth(*t++);
        text(t, FX + 5, by + 11);
        rbox(FX + 1, by + 1, FX + FW - 2, by + 14);
        if (foc == k)
            WInvertRectangle(FX + 6 + w, by + 3, FX + 7 + w, by + 12);
    } else {
        /* an option: its box, ticked or not */
        x -= 14;
        WRectangle(x, by + 3, x + 9, by + 12);
        rbox(x, by + 3, x + 9, by + 12);
        if (k == 7 ? pgs.flags & PS_NOFIRST : def_on)
            WInvertRectangle(x + 2, by + 5, x + 7, by + 10);
        x += 14;
    }
    /* the label, lit when it has the focus */
    w = width(lab[k]);
    WRectangle(x - 2, by + 2, x + w + 1, by + 13);
    text(lab[k], x, by + 11);
    if (foc == k)
        WInvertRectangle(x - 2, by + 2, x + w + 1, by + 13);
    UseFont(0);
}

static void __fastcall__ say(const char *s)
{
    ui();
    WRectangle(PG_L, YM - 10, PG_R, TA_END);
    text(s, LX, YM);
    UseFont(0);
}

static const char hint[] = "Tab: next   Left, right: change   Return: OK   Esc: Cancel";

static void draw(void)
{
    unsigned char k;
    ui();
    WRectangle(PG_L, PP_Y, PG_R, TA_END);
    WHorizontalLine(0xFF, PG_L, PG_R, PP_Y);
    ColorRect(PG_C0, PG_CW, PP_Y >> 3, (TA_END + 1 - PP_Y) >> 3, C_BAR);
    by = Y4;
    button(OKX, 56, "OK", 0);
    rbox(OKX + 2, Y4 + 2, OKX + 53, Y4 + 13);       /* (the default: a double frame) */
    button(CNX, 64, "Cancel", 0);
    for (k = 0; k < NIT; ++k)
        if (k < 1 || k > 4)
            item(k);
    ui();
    text("A header or footer: centre, or left|right, or left|centre|right;", LX, YH);
    text("{page} {pages} {date} {name} are filled in.", LX, YH + 12);
    UseFont(0);
    say(hint);
}

void pp_open(void)
{
    unsigned char k = foc;
    view_cursor(0);
    foc = t_arg;
    if (pp_on) {
        item(k);
        item(foc);
        return;
    }
    fp_on = 0;                          /* (the find panel's place: this one's now) */
    memcpy(&bak, &pgs, PGS_N);
    def_on = 0;
    pp_on = 1;
    ta_bot = PP_Y - 1;
    draw();
    view_all();
    follow = 1;
    need_view = 1;
}

static void close(void)
{
    pp_on = 0;
    ta_bot = TA_END;
    ColorRect(PG_C0, PG_CW, PP_Y >> 3, (TA_END + 1 - PP_Y) >> 3, 0);    /* (the page's paper) */
    view_all();
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void ok(void)
{
    if (memcmp(&pgs, &bak, PGS_N)) {
        pg_apply();
        doc_modified = 1;
        if (memcmp(&pgs, &bak, 14)) {
            /* the paper or the margins: the text wrapped again (all of
               a document being opened read first) */
            SetPointer(PTR_BUSY);
            if (ld_kind)
                ld_need(0xFFFFFFFFUL);
            lay_busy = busy_ptr;
            lay_all();
            lay_busy = 0;
            top_line = 0;
        }
    }
    if (def_on)
        savedef();
    close();
}

static void cancel(void)
{
    memcpy(&pgs, &bak, PGS_N);
    pg_apply();
    close();
}

static void __fastcall__ focus(unsigned char k)
{
    unsigned char o = foc;
    foc = k;
    item(o);
    item(k);
}

/* the paper k: the margins kept, unless the text would be too wide or
   too narrow for it (then as near as they can be, or the paper's own) */
static void __fastcall__ paper(unsigned char k)
{
    static unsigned long t;
    static unsigned short dm;
    pgs.paper = k;
    pgs.w = k ? 12240 : 11906;          /* (A4, Letter: page.c's) */
    pgs.h = k ? 15840 : 16838;
    dm = k == PP_LETTER ? 1440 : 1417;
    t = (unsigned long)pgs.w - pgs.left - pgs.right;
    if ((long)t < TXW_MIN * TW_PX)
        pgs.left = pgs.right = dm;
    else if (t > TXW_MAX * TW_PX) {
        t = (t - TXW_MAX * TW_PX + 1) >> 1;
        pgs.left += (unsigned short)t;
        pgs.right += (unsigned short)t;
    }
    if ((long)pgs.h - pgs.top - pgs.bottom < TXH_MIN * TW_PX)
        pgs.top = pgs.bottom = dm;
    item(0);
}

/* margin k one step (d: 1 or -1) */
static void __fastcall__ step(unsigned char k, signed char d)
{
    static unsigned short *m, o;
    static unsigned long v, t;
    m = mg(k);
    if (pgs.paper == PP_LETTER) {
        v = (*m + 36) / 72;
        if (d < 0 && !v)
            return;
        v = (v + d) * 72;
    } else {
        v = ((unsigned long)*m * 254 + 7200) / 14400;
        if (d < 0 && !v)
            return;
        v = ((v + d) * 14400 + 127) / 254;
    }
    o = k == 1 ? pgs.bottom : k == 2 ? pgs.top : k == 3 ? pgs.right : pgs.left;
    t = (k < 3 ? pgs.h : pgs.w) - v - o;
    if (v > 14400 || (long)t < (k < 3 ? TXH_MIN : TXW_MIN) * TW_PX) {
        say(k < 3 ? "The text must be 3 inches high at least." : "The text must be 3 inches wide at least.");
        return;
    }
    if (k >= 3 && t > TXW_MAX * TW_PX) {
        say("The text can be 6.6 inches wide at most (the screen's).");
        return;
    }
    *m = (unsigned short)v;
    item(k);
    say(hint);
}

static void __fastcall__ change(signed char d)
{
    if (foc == 0)
        paper(pgs.paper == PP_LETTER ? PP_A4 : PP_LETTER);
    else if (foc < 5)
        step(foc, d);
    else if (foc == 7) {
        pgs.flags ^= PS_NOFIRST;
        item(7);
    } else if (foc == 8) {
        def_on ^= 1;
        item(8);
    }
}

void pp_key(void)
{
    unsigned char k = keyData, m = keyDataMods, n;
    char *s = foc == 5 ? pgs.head : pgs.foot;
    switch (k) {
    case KEY_ESC:
        cancel();
        return;
    case CR:
    case 0x8D:
        ok();
        return;
    case KEY_TAB:
        focus(m & 1 ? (foc ? foc - 1 : NIT - 1) : (foc + 1) % NIT);
        return;
    }
    if (m & KEYM_ARROW) {
        if (k == KEY_UP)
            focus(foc ? foc - 1 : NIT - 1);
        else if (k == KEY_DOWN)
            focus((foc + 1) % NIT);
        else
            change(k == KEY_LEFT ? -1 : 1);
        return;
    }
    if (m & 6)
        return;                         /* (Ctrl, Alt: nothing here) */
    if (foc != 5 && foc != 6) {
        if (k == ' ')
            change(1);
        return;
    }
    n = (unsigned char)strlen(s);
    if (k == 8 || k == KEY_DELETE) {
        if (n)
            s[n - 1] = 0;
    } else {
        if (k == 0xA0)
            k = ' ';
        if (!((k >= 32 && k < 127) || k >= 160) || n >= HF_MAX)
            return;
        s[n] = k;
        s[n + 1] = 0;
    }
    item(foc);
}

void pp_press(void)
{
    static unsigned int x, y;
    static unsigned char k, r;
    x = r11;
    y = r12;
    if (y < Y0 || x < PG_L || x > PG_R)
        return;
    r = (unsigned char)((y - Y0) / 24);
    if (y - Y0 - r * 24 >= 16)
        return;
    if (r == 0 && x >= FX && x < FX + 104) {
        focus(0);
        paper(x < FX + 48 ? PP_A4 : PP_LETTER);
        return;
    }
    if (r < 2)
        for (k = 1; k < 5; ++k)
            if (ly[k] == Y0 + r * 24 && x >= lx[k] && x < lx[k] + 128) {
                focus(k);
                if (x >= lx[k] + 48 && x < lx[k] + 64)
                    step(k, -1);
                else if (x >= lx[k] + 112)
                    step(k, 1);
                return;
            }
    if (r == 2 || r == 3) {
        if (x >= FX)
            focus(5 + r - 2);
        return;
    }
    if (r == 4) {
        if (x >= OKX && x < OKX + 56)
            ok();
        else if (x >= CNX && x < CNX + 64)
            cancel();
        else if (x >= FX && x < MX0 - 8) {
            focus(7);
            change(1);
        } else if (x >= MX0 && x < OKX - 8) {
            focus(8);
            change(1);
        }
    }
}

/*---------------------------------------------------------------
  File > Export as PDF
 ---------------------------------------------------------------*/
extern unsigned char pdf_fh;            /* (pdfx.c: main RAM) */
static char dir[64];

void t_pdf(void)
{
    static unsigned char e, h;
    char *p;
    if (pp_on)
        return;
    if (ld_kind)
        ld_need(0xFFFFFFFFUL);          /* (all of it in first) */
    /* the name: the document's, its extension .pdf */
    strcpy(name, docname[0] ? docname : docname[1] ? docname + 1 : "Untitled");
    p = strrchr(name, '.');
    if (p && strlen(p) <= 4)
        *p = 0;
    name[58] = 0;
    strcat(name, ".pdf");
    strcpy(dir, (char *)curDirName);
    strcpy((char *)curDirName, vr_cd(VR_DOCS));
    for (;;) {
        if (!file_dlg(VRF_SAVE, "Export as PDF", name))
            goto back;
        if (!strchr(name, '.') && strlen(name) < 59)
            strcat(name, ".pdf");
        h = FOpen(name, 'r');
        if (h < 2)
            break;
        FClose(h);
        if (yesno("Replace the file", name))
            break;
    }
    pdf_fh = FOpen(name, 'w');
    if (pdf_fh < 2) {
        message("Cannot write in this folder:", (char *)curDirName);
        goto back;
    }
    SetPointer(PTR_BUSY);
    MARK("pdf<");
    pg_arg = 1;
    pg_first();                         /* (every page counted: {pages}) */
    pdf_pages = pg_tot;
    pdf_title = name;
    e = pdf_write();
    if (FClose(pdf_fh) && !e)
        e = PE_IO;
    MARK("pdf>");
    if (e)
        FDelete(name);
#ifdef CW_TEST
    {
        static char t[] = "CW PDF x";
        t[7] = '0' + e;
        LogText(t);
    }
#endif
    if (e == PE_STOP)
        set_status("Export stopped");
    else if (e == PE_MEM)
        message("There is not enough memory", "for the PDF's table.");
    else if (e)
        message("The PDF could not be written", "(is the card full?)");
    else {
        p = num(status_msg, pdf_page);
        strcpy(p, pdf_page == 1 ? " page exported" : " pages exported");
        status_dirty = 1;
    }
back:
    strcpy((char *)curDirName, dir);
    ChDir((char *)curDirName);
    SetPointer(PTR_ARROW);
    need_view = 1;
    status_dirty = 1;
}
