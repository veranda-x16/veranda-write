/* Veranda Write on Veranda: Tools > Spelling options (overlay 24, with
   the suggestions and spelling's files: overlay 26, the Tools menu's,
   has not the room in the test build; W60).

   A panel under the text where Check spelling's is (the view's bottom
   moves up to SPP_Y - 1; spp_on 2: v/spui.c hands it the keys and the
   clicks), in the look of the page setup panel (v/pgpanel.c):

     [x] Check spelling as you work    off: only when asked (F7, F8, the
                                       Tools menu); nothing is loaded or
                                       marked before, a right click is
                                       not a word's suggestions
     [x] Mark misspellings as you type (ticked only while the first is:
                                       ticking it ticks that too)
     New documents  [English] [Polish] [Keyboard's]
                                       the language of new documents and
                                       of those saved without one (English
                                       until another is chosen)
                                                         [OK] [Cancel]

   Tab and the up and down arrows move between the three, Space and the
   left and right arrows change one, Return is OK (the settings kept in
   /VERANDA/.SYSTEM/WRITE.CFG: v/spload.c), Esc Cancel. Tools > Mark
   misspellings (v/sppop.c) changes the second and saves at once.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"
#pragma code-name ("OVERLAY24")
#pragma rodata-name ("OVL24RO")
#pragma bss-name ("OVL24BSS")
#pragma data-name ("OVL24DATA")

extern char run[100];                   /* (view.c: main RAM, what the kernel draws) */
#define SPP_Y  336                      /* (v/spui.c's panel: card rows 42-57) */
#define R0     (SPP_Y + 8)              /* as you work */
#define R1     (SPP_Y + 32)             /* the marks */
#define R2     (SPP_Y + 56)             /* the language */
#define R3     (SPP_Y + 80)             /* OK, Cancel */
#define YH     (SPP_Y + 108)            /* the explanation's baseline */
#define YM     (SPP_Y + 122)            /* the keys' */
#define LX     (PG_L + 24)              /* the options' labels (their boxes 14 before) */
#define FX     184                      /* the languages' buttons */
#define OKX    456
#define CNX    520

static unsigned char v, foc;            /* the settings being changed (sp_cfg's form); the item */

static const char *const lab[3] = { "Check spelling as you work", "Mark misspellings as you type", "New documents" };
static const unsigned int ly[3] = { R0, R1, R2 };
static const char *const lname[3] = { "English", "Polish", "Keyboard's" };    /* (LANG_EN, LANG_PL, SPC_KEYB) */
static const unsigned char lbx[3] = { 0, 72, 144 };     /* (from FX) */
static const unsigned char lbw[3] = { 64, 64, 88 };
static const char *const expl[3] = {
    "Off: spelling only when you ask (F7, F8, the Tools menu).",
    "A dotted line under the words the dictionary has not.",
    "Also for those saved without one. Keyboard's: Polish if Veranda's is.",
};
static const char hint[] = "Tab: next   Space, left, right: change   Return: OK   Esc: Cancel";

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

static void ui(void)
{
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    UseFont(FONT_UI);
    currentMode = 0;
    SetPattern(0);
}

/* a box w wide at x on row y, s centred in it; on: lit */
static void __fastcall__ button(unsigned int x, unsigned int y, unsigned char w, const char *s, unsigned char on)
{
    ColorRect(x >> 3, w >> 3, y >> 3, 2, 0);
    WRectangle(x, y, x + w - 1, y + 15);
    text(s, x + ((w - width(s)) >> 1), y + 11);
    rbox(x + 1, y + 1, x + w - 2, y + 14);
    if (on)
        WInvertRectangle(x + 2, y + 2, x + w - 3, y + 13);
}

static void say(void)
{
    WRectangle(PG_L, YH - 10, PG_R, TA_END);
    text(expl[foc], LX - 14, YH);
    text(hint, LX - 14, YM);
}

/* item k drawn (its label lit when it has the focus) */
static void __fastcall__ item(unsigned char k)
{
    static unsigned int x, y, w;
    static unsigned char j;
    ui();
    y = ly[k];
    x = LX;
    if (k == 2) {
        x = LX - 14;
        for (j = 0; j < 3; ++j)
            button(FX + lbx[j], y, lbw[j], lname[j], (v & SPC_LANG) == j + 1);
    } else {
        WRectangle(LX - 14, y + 3, LX - 5, y + 12);
        rbox(LX - 14, y + 3, LX - 5, y + 12);
        if (k == 0 ? !(v & SPC_ASK) : !(v & (SPC_ASK | SPC_NOMARK)))
            WInvertRectangle(LX - 12, y + 5, LX - 7, y + 10);
    }
    w = width(lab[k]);
    WRectangle(x - 2, y + 2, x + w + 1, y + 13);
    text(lab[k], x, y + 11);
    if (foc == k)
        WInvertRectangle(x - 2, y + 2, x + w + 1, y + 13);
    UseFont(0);
}

static void draw(void)
{
    static unsigned char k;
    ui();
    WRectangle(PG_L, SPP_Y, PG_R, TA_END);
    WHorizontalLine(0xFF, PG_L, PG_R, SPP_Y);
    ColorRect(PG_C0, PG_CW, SPP_Y >> 3, (TA_END + 1 - SPP_Y) >> 3, C_BAR);
    button(OKX, R3, 56, "OK", 0);
    rbox(OKX + 2, R3 + 2, OKX + 53, R3 + 13);      /* (the default: a double frame) */
    button(CNX, R3, 64, "Cancel", 0);
    for (k = 0; k < 3; ++k)
        item(k);
    ui();
    say();
    UseFont(0);
}

void so_open(void)
{
    if (pp_on)
        return;
    view_cursor(0);
    if (fp_on) {
        fp_on = 0;                      /* (one panel at a time: Find's goes) */
        ColorRect(PG_C0, PG_CW, FP_Y >> 3, (TA_END + 1 - FP_Y) >> 3, 0);
    }
    v = sp_cfg;
    foc = 0;
    spp_on = 2;                         /* (Check spelling's panel, if open, gives its place) */
    ta_bot = SPP_Y - 1;
    draw();
    view_all();
    follow = 1;
    need_view = 1;
}

void so_close(void)
{
    spp_on = 0;
    ta_bot = TA_END;
    ColorRect(PG_C0, PG_CW, SPP_Y >> 3, (TA_END + 1 - SPP_Y) >> 3, 0);    /* (the page's paper) */
    view_all();                         /* (the marks drawn as the settings say) */
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

/* sp_cfg kept (sp_live from it) and written (t_arg 0: done) */
void so_save(void)
{
    callb(bank22, sp_cfgsave);
    if (t_arg)
        message("The spelling settings could not be", "written (is the card full?)");
}

static void ok(void)
{
    if (v != sp_cfg) {
        sp_cfg = v;
        so_save();
    }
    so_close();
    set_status("Spelling options kept");
}

static void __fastcall__ focus(unsigned char k)
{
    static unsigned char o;
    o = foc;
    foc = k;
    item(o);
    item(k);
    ui();
    say();
    UseFont(0);
}

/* item foc changed: d 1 on, -1 back (the language) */
static void __fastcall__ change(signed char d)
{
    if (foc == 0)
        v ^= SPC_ASK;
    else if (foc == 1)
        v = v & (SPC_ASK | SPC_NOMARK) ? v & ~(SPC_ASK | SPC_NOMARK) : v | SPC_NOMARK;
    else
        v = (v & ~SPC_LANG) | (unsigned char)(((v & SPC_LANG) + 2 + d) % 3 + 1);
    item(0);
    item(1);
    item(2);
}

void so_key(void)
{
    static unsigned char k, m;
    k = keyData;
    m = keyDataMods;
    t_arg = 1;                          /* (every key the panel's) */
    switch (k) {
    case KEY_ESC:
        so_close();
        return;
    case CR:
    case 0x8D:
        ok();
        return;
    case KEY_TAB:
        focus(m & 1 ? (foc + 2) % 3 : (foc + 1) % 3);
        return;
    case ' ':
        change(1);
        return;
    }
    if (m & KEYM_ARROW) {
        if (k == KEY_UP)
            focus((foc + 2) % 3);
        else if (k == KEY_DOWN)
            focus((foc + 1) % 3);
        else
            change(k == KEY_LEFT ? -1 : 1);
    }
}

void so_press(void)
{
    static unsigned int x, y;
    static unsigned char j;
    x = r11;
    y = r12;
    if (y >= R3 && y < R3 + 16) {
        if (x >= OKX && x < OKX + 56)
            ok();
        else if (x >= CNX && x < CNX + 64)
            so_close();
        return;
    }
    for (j = 0; j < 3; ++j)
        if (y >= ly[j] && y < ly[j] + 16)
            break;
    if (j == 3)
        return;
    if (j < 2) {
        if (x >= LX - 16 && x < LX + 240) {
            focus(j);
            change(1);
        }
        return;
    }
    for (j = 0; j < 3; ++j)
        if (x >= FX + lbx[j] && x < FX + lbx[j] + lbw[j]) {
            focus(2);
            v = (v & ~SPC_LANG) | (j + 1);
            item(2);
            return;
        }
}
