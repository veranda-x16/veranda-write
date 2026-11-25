/* Veranda Write on Veranda: Check spelling's panel, the suggestions'
   pop-up and the Tools menu (overlay 25; W57-W59).

   Tools > Check spelling (F7) opens a panel under the text, as Find and
   Replace does (the view's bottom moves up to SPP_Y - 1): the next word
   from the cursor that the dictionary has not (to the document's end,
   then from its start to where it began) is selected in the text and
   shown in its context; up to six suggestions; "Change to" (the first
   suggestion, in the word's case; typing edits it); Change (Return),
   Change all (every place the word is written so, one undo step),
   Ignore (this place), Ignore all (this session), Add (to the user's
   words, /VERANDA/.SYSTEM/WRITE.UDIC), Close (Esc). Alt + the
   underlined... the buttons' first letters (Alt+C, L, I, G, A), Alt+1-6
   a suggestion, Up and Down go through them. The text in Gate Sans 12
   (its glyphs $A0-$FF: Polish), drawn by Write's renderer as the
   header and footer are (W54); the labels and buttons in the UI font.

   A right click on a word, or F8 for the cursor's, opens a pop-up under
   it: its first suggestions, Add to dictionary, Ignore all (drawn on the
   front screen only, what it covered back from the back screen, as the
   toolbar's pop-ups: popup.c).

   Tools: Check spelling, Spelling suggestions, Mark misspellings as you
   type (on or off), English and Polish (the document's language: kept
   in it, CWRD 1.5; a new one's the settings', W60: English unless
   Tools > Spelling options says Polish or the keyboard's).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"
#pragma code-name ("OVERLAY25")
#pragma rodata-name ("OVL25RO")
#pragma bss-name ("OVL25BSS")
#pragma data-name ("OVL25DATA")

extern char run[100];                   /* (view.c: main RAM, the renderer reads it) */
#define SPP_Y  336                      /* the panel: card rows 42-57 */
#define R1     (SPP_Y + 8)              /* the context */
#define R2     (SPP_Y + 32)             /* Change to; Change, Change all */
#define R3     (SPP_Y + 56)             /* suggestions 1-3; Ignore, Ignore all */
#define R4     (SPP_Y + 80)             /* suggestions 4-6; Add, Close */
#define RM     (SPP_Y + 115)            /* the message's baseline */
#define FL     56                       /* the fields' left, right */
#define FR     583
#define CX     136                      /* Change to's field */
#define CW     288
#define BX     440                      /* the buttons: two a row, 68 wide */
#define BW     68
#define SX     56                       /* the suggestions: three a row, 128 apart */
#define SW     120
#define NSH    6                        /* suggestions shown */

#include "spdraw.h"

/*---------------------------------------------------------------
  the panel
 ---------------------------------------------------------------*/
static const char *const bname[6] = { "Change", "Change all", "Ignore", "Ignore all", "Add", "Close" };

static void __fastcall__ button(unsigned char k)
{
    static unsigned int x, y;
    x = BX + (k & 1) * (BW + 8);
    y = k < 2 ? R2 : k < 4 ? R3 : R4;
    ColorRect(x >> 3, BW >> 3, y >> 3, 2, 0);
    label(bname[k], x + ((BW - uw(bname[k])) >> 1), y + 11);
    rbox(x + 1, y + 1, x + BW - 2, y + 14);
    if (!k)
        rbox(x + 2, y + 2, x + BW - 3, y + 13);   /* (Return's: a double frame) */
}

/* the context: up to 40 bytes either side of the word, in its paragraph */
static void context(void)
{
    static pos_t a, p;
    static unsigned int n, k, ia, ib, w;
    static unsigned char c, j;
    a = fws > 40 ? fws - 40 : 0;
    n = cb_read(&tx, a, iobuf, (unsigned int)(fwe - a) + 40);
    j = 0;
    ia = ib = 0xFFFF;
    for (k = 0; k < n && j < 90;) {
        p = a + k;
        if (p == fws)
            ia = j;
        if (p == fwe)
            ib = j;
        c = iobuf[k];
        if (IS_KIND(c)) {
            k += 3;
            continue;
        }
        ++k;
        if (c == CH_CR) {
            if (ia == 0xFFFF) {
                j = 0;                  /* (the paragraph before: not shown) */
                continue;
            }
            break;
        }
        run[j++] = c == CH_TAB ? ' ' : (char)c;
    }
    if (ib == 0xFFFF)
        ib = j;
    if (ia == 0xFFFF)
        ia = 0;
    run[j] = 0;
    /* (cut to the field: the side with more of it) */
    for (;;) {
        w = gs ? gw(run) : uw(run);
        if (w <= FR - FL - 12 || j <= ib - ia)
            break;
        if (ia > j - ib) {
            memmove(run, run + 1, j);
            --ia;
            --ib;
        } else
            run[ib + (j - ib) - 1] = 0;
        j = (unsigned char)strlen(run);
    }
    put(run, FL + 6);
    blit();
    memcpy(iobuf, run, j + 1);
    if (st == 1) {
        iobuf[ia] = 0;
        w = gs ? gw((char *)iobuf) : uw((char *)iobuf);
        memcpy(iobuf, run, j + 1);
        iobuf[ib] = 0;
        n = gs ? gw((char *)iobuf + ia) : uw((char *)iobuf + ia);
        WInvertRectangle(FL + 6 + w - 1, R1 + 1, FL + 6 + w + n, R1 + 14);
    }
}

static void draw(void)
{
    static unsigned char k;
    static unsigned int x, y;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(PG_L, SPP_Y, PG_R, TA_END);
    WHorizontalLine(0xFF, PG_L, PG_R, SPP_Y);
    ColorRect(PG_C0, PG_CW, SPP_Y >> 3, (TA_END + 1 - SPP_Y) >> 3, C_BAR);
    ColorRect(FL >> 3, (FR + 1 - FL) >> 3, R1 >> 3, 2, 0);
    ColorRect(CX >> 3, CW >> 3, R2 >> 3, 2, 0);
    for (k = 0; k < NSH && k < ns; ++k)
        ColorRect((SX + (k % 3) * 128) >> 3, SW >> 3, (k < 3 ? R3 : R4) >> 3, 2, 0);
    gs_font();
    /* the context */
    band(R1);
    if (st == 1)
        context();
    else
        blit();
    rbox(FL + 1, R1 + 1, FR - 1, R1 + 14);
    /* Change to */
    band(R2);
    put(chg, CX + 6);
    blit();
    /* the suggestions */
    for (y = R3; y <= R4; y += R4 - R3) {
        band(y);
        for (k = y == R3 ? 0 : 3; k < ns && k < (y == R3 ? 3 : NSH); ++k) {
            x = SX + (k % 3) * 128;
            run[0] = '1' + k;
            run[1] = ' ';
            strcpy(run + 2, sg[k]);
            put(run, x + 5);
        }
        blit();
    }
    UseFont(FONT_UI);
    currentMode = 0;
    label("Change to", FL, R2 + 11);
    rbox(CX + 1, R2 + 1, CX + CW - 2, R2 + 14);
    for (k = 0; k < ns; ++k) {
        x = SX + (k % 3) * 128;
        y = k < 3 ? R3 : R4;
        rbox(x + 1, y + 1, x + SW - 2, y + 14);
        if (k == hi)
            WInvertRectangle(x + 2, y + 2, x + SW - 3, y + 13);
    }
    if (st == 1 && !ns)
        label("(no suggestions)", SX + 5, R3 + 11);
    for (k = 0; k < 6; ++k)
        button(k);
    label(msg, FL, RM);
    UseFont(0);
}

static const char m_en[] = "Not in the English dictionary.   Return: change   Esc: close";
static const char m_pl[] = "Not in the Polish dictionary.   Return: change   Esc: close";
/* (W61) the dictionaries are not in the package: where they come from */
static const char m_noen[] = "No English dictionary: Tools > Get dictionaries, or copy EN.DIC to /VERANDA/DICT.";
static const char m_nopl[] = "No Polish dictionary: Tools > Get dictionaries, or copy PL.DIC to /VERANDA/DICT.";

#ifdef CW_TEST
/* "CW SPELL r word n first next sugg": what was found (the word and the
   first suggestion in hex), the cycles of the search and of the
   suggestions (the gate: tools/check.py) */
static unsigned long clk(void)
{
    static unsigned long c;
    ((unsigned char *)&c)[0] = *(volatile unsigned char *)0x9FB8;
    ((unsigned char *)&c)[1] = *(volatile unsigned char *)0x9FB9;
    ((unsigned char *)&c)[2] = *(volatile unsigned char *)0x9FBA;
    ((unsigned char *)&c)[3] = *(volatile unsigned char *)0x9FBB;
    return c;
}
static unsigned long t0, t1, t2;
static char *__fastcall__ hx(char *p, unsigned char v)
{
    static const char h[] = "0123456789ABCDEF";
    *p++ = h[v >> 4];
    *p++ = h[v & 15];
    return p;
}
static char *__fastcall__ hl(char *p, unsigned long v)
{
    p = hx(p, (unsigned char)(v >> 24));
    p = hx(p, (unsigned char)(v >> 16));
    p = hx(p, (unsigned char)(v >> 8));
    p = hx(p, (unsigned char)v);
    *p++ = ' ';
    return p;
}
static void __fastcall__ log_found(unsigned char r)
{
    static char *p;
    static unsigned char k;
    p = (char *)iobuf;
    strcpy(p, "CW SPELL ");
    p = hx(p + 9, r);
    *p++ = ' ';
    for (k = 0; k < wdl && r == 1; ++k)
        p = hx(p, wd[k]);
    *p++ = ' ';
    p = hx(p, ns);
    *p++ = ' ';
    for (k = 0; ns && sg[0][k]; ++k)
        p = hx(p, (unsigned char)sg[0][k]);
    *p++ = ' ';
    p = hl(p, t1 - t0);
    p = hl(p, t2 - t1);
    *p = 0;
    LogText(iobuf);
}
#endif

/* the next word the dictionary has not, from sp_from */
static void next(void)
{
    static unsigned char r;
    SetPointer(PTR_BUSY);
#ifdef CW_TEST
    t0 = clk();
#endif
    r = sp_next();
#ifdef CW_TEST
    t1 = t2 = clk();
#endif
    ns = 0;
    hi = 0xFF;
    chg[0] = 0;
    if (r == 1) {
        st = 1;
        wdl = sp_wl;
        memcpy(wd, sp_word, wdl);
        wd[wdl] = 0;
        anc = fws = sp_ws;
        cur = fwe = sp_we;
        ui_moved();
        suggestions();
#ifdef CW_TEST
        t2 = clk();
#endif
        if (ns) {
            strcpy(chg, sg[0]);
            hi = 0;
        } else
            strcpy(chg, (char *)wd);
        msg = fmt_lang == LANG_PL ? m_pl : m_en;
    } else {
        st = r == 3 ? 3 : 2;
        anc = cur;
        ui_moved();
        msg = r == 3 ? (fmt_lang == LANG_PL ? m_nopl : m_noen)
              : r == 2 ? "Stopped (Esc)." : nchanged ? "The spelling is checked (changes made)." : "The spelling is checked.";
    }
    SetPointer(PTR_ARROW);
    need_view = 1;
    follow = 1;
#ifdef CW_TEST
    log_found(r);
#endif
    if (spp_on)
        draw();
}

void spp_open(void)
{
    static pos_t p;
    view_cursor(0);
    if (!spp_on) {
        if (fp_on) {
            fp_on = 0;                  /* (one panel at a time: Find's goes) */
            ColorRect(PG_C0, PG_CW, FP_Y >> 3, (TA_END + 1 - FP_Y) >> 3, 0);
        }
        spp_on = 1;
        ta_bot = SPP_Y - 1;
        nchanged = 0;
        /* from the selection's end, or the start of the word the cursor is in */
        p = HAS_SEL() ? SEL_HI() : cur;
        if (!HAS_SEL()) {
            lt_find(p);
            sp_tokat(lf_pos, p + SP_MAXW + 1);
            while (sp_tok())
                if (sp_ws <= p && p <= sp_we) {
                    p = sp_ws;
                    break;
                } else if (sp_ws > p)
                    break;
        }
        sp_from = sp_stop = p;
        sp_wrap = 0;
        st = 0;
        ns = 0;
        chg[0] = 0;
        msg = "";
        draw();
        view_all();
    } else if (st != 1)
        sp_wrap = 0;
    sp_drop(2);                         /* (W61: a dictionary that came since is found now) */
    next();
}

void spp_close(void)
{
    spp_on = 0;
    ta_bot = TA_END;
    ColorRect(PG_C0, PG_CW, SPP_Y >> 3, (TA_END + 1 - SPP_Y) >> 3, 0);
    view_all();
    need_view = 1;
}

/* the word is still where it was found, selected */
static unsigned char there(void)
{
    return st == 1 && SEL_LO() == fws && SEL_HI() == fwe;
}

static void change(void)
{
    static pos_t e;
    static long d;
    if (!there())
        return;
    ul_step(U_CMD, cur, anc);
    e = change_at(fws, fwe);
    d = (long)(e - fws) - (long)(fwe - fws);
    if (sp_wrap && sp_stop > fws)
        sp_stop += d;
    cur = anc = e;
    sp_from = e;
    ++nchanged;
    ui_moved();
    next();
}

/* every place the word is written so: one undo step */
static void change_all(void)
{
    static pos_t p, q, e;
    static unsigned int n;
    static long d;
    if (!there())
        return;
    SetPointer(PTR_BUSY);
    ul_step(U_CMD, cur, anc);
    n = 0;
    sp_tokat(0, DOC_LEN);
    while (sp_tok()) {
        if (sp_wl != wdl || memcmp(sp_word, wd, wdl))
            continue;
        p = sp_ws;
        q = sp_we;
        e = change_at(p, q);
        d = (long)(e - p) - (long)(q - p);
        if (sp_stop > p)
            sp_stop += d;
        if (sp_from > p)
            sp_from += d;
        ++n;
        sp_tokat(e, DOC_LEN);
    }
    nchanged += n;
    cur = anc = sp_from;
    ui_moved();
    view_all();
    next();
}

static void ignore_all(void)
{
    if (st != 1)
        return;
    sp_ignore();                        /* (sp_lw: the word in lower case, from sp_next) */
    view_all();                         /* (its marks gone) */
    next();
}

static void add(void)
{
    if (st != 1)
        return;
    if (sp_adduser())
        message("The word could not be added", "(the user's words full, or the card).");
    view_all();
    next();
}

static void __fastcall__ choose(unsigned char k)
{
    if (k >= ns)
        return;
    hi = k;
    strcpy(chg, sg[k]);
    draw();
}

static void __fastcall__ act(unsigned char k)
{
    if (st != 1 && k != 5) {
        if (k == 2 || k == 0)
            next();                     /* (Return after the end: from the start again) */
        return;
    }
    if (k < 2 && !there()) {
        sp_from = cur;                  /* (the text changed meanwhile: from the cursor) */
        next();
        return;
    }
    word_again();
    switch (k) {
    case 0: change(); break;
    case 1: change_all(); break;
    case 2: next(); break;
    case 3: ignore_all(); break;
    case 4: add(); break;
    case 5: spp_close(); break;
    }
}

void spp_key(void)
{
    static unsigned char k, m, i;
    if (spp_on == 2) {
        callb(bank24, so_key);          /* (W60: the options' panel) */
        return;
    }
    k = keyData;
    m = keyDataMods;
    t_arg = 1;
    if (m & 4) {                        /* Alt: the buttons, the suggestions */
        switch (k & 0x5F) {
        case 'C': act(0); return;
        case 'L': act(1); return;
        case 'I': act(2); return;
        case 'G': act(3); return;
        case 'A': act(4); return;
        }
        if ((k & 0x7F) >= '1' && (k & 0x7F) < '1' + ns) {
            choose((k & 0x7F) - '1');
            act(0);
            return;
        }
        t_arg = 0;
        return;
    }
    if (m & KEYM_ARROW) {
        if ((k == KEY_UP || k == KEY_DOWN) && ns) {
            choose(hi == 0xFF ? 0 : k == KEY_DOWN ? (hi + 1) % ns : (hi + ns - 1) % ns);
            return;
        }
        t_arg = 0;
        return;
    }
    if (m & 2) {
        t_arg = 0;                      /* (the commands: the document's) */
        return;
    }
    i = (unsigned char)strlen(chg);
    switch (k) {
    case KEY_ESC:
        spp_close();
        return;
    case KEY_F7:
        act(2);
        return;
    case CR:
    case 0x8D:
        act(0);
        return;
    case 8:
    case KEY_DELETE:
        if (i) {
            chg[i - 1] = 0;
            hi = 0xFF;
            draw();
        }
        return;
    case 0xA0:
        k = ' ';
        break;
    }
    if (((k >= 32 && k < 127) || k >= 160) && i < SP_MAXW) {
        chg[i] = k;
        chg[i + 1] = 0;
        hi = 0xFF;
        draw();
        return;
    }
    t_arg = 0;
}

void spp_press(void)
{
    static unsigned int x, y;
    static unsigned char k;
    if (spp_on == 2) {
        callb(bank24, so_press);        /* (W60) */
        return;
    }
    x = r11;
    y = r12;
    if (x >= BX && y >= R2 && y < R4 + 16) {
        k = (y >= R4 ? 4 : y >= R3 ? 2 : 0) + (x >= BX + BW + 8);
        if (y >= R2 + 16 && y < R3)
            return;
        if (y >= R3 + 16 && y < R4)
            return;
        act(k);
        return;
    }
    if (x >= SX && x < SX + 3 * 128 && ((y >= R3 && y < R3 + 16) || (y >= R4 && y < R4 + 16))) {
        k = (x - SX) >> 7;
        if (y >= R4)
            k += 3;
        if (k < ns) {
            if (k == hi && dblClickCount) {
                dblClickCount = 0;
                act(0);                 /* (a double click: changed) */
                return;
            }
            choose(k);
            dblClickCount = 30;
        }
    }
}

