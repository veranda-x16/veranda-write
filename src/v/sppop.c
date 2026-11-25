/* Veranda Write on Veranda: the suggestions' pop-up and the Tools menu
   (overlay 26; W57-W59; v/spui.c says what they do; the panel's
   helpers in v/spdraw.h, compiled into both).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"
#include <vrpath.h>
#pragma code-name ("OVERLAY26")
#pragma rodata-name ("OVL26RO")
#pragma bss-name ("OVL26BSS")
#pragma data-name ("OVL26DATA")

extern char run[100];                   /* (view.c: main RAM, the renderer reads it) */
#define SPP_Y  336
#define NSH    6

#include "spdraw.h"

/*---------------------------------------------------------------
  the pop-up (a right click on a word, F8)
 ---------------------------------------------------------------*/
static unsigned int pl, pr, pt, pb;
static unsigned char pn, ph;

/* item i's row of the front screen: the band in the row bank (rr_y) */
static void __fastcall__ front(unsigned int y)
{
    static unsigned char r, c0, nc, k;
    static unsigned int a;
    c0 = (pl - PG_L) >> 3;
    nc = (pr + 1 - pl) >> 3;
    if (nc > sizeof sp_buf)
        nc = sizeof sp_buf;
    for (r = 0; r < BAND_HF; ++r) {
        bk_rd(rr_bank, 0xA000u + r * 68 + c0, sp_buf, nc);
        a = (y + r) * 80 + PG_C0 + c0;
        *(volatile unsigned char *)0x9F25 = 0;
        *(volatile unsigned char *)0x9F20 = (unsigned char)a;
        *(volatile unsigned char *)0x9F21 = (unsigned char)(a >> 8);
        *(volatile unsigned char *)0x9F22 = 0x10;
        for (k = 0; k < nc; ++k)
            *(volatile unsigned char *)0x9F23 = sp_buf[k];
    }
}

static const char *const pitem[2] = { "Add to dictionary", "Ignore all" };

static void __fastcall__ plight(unsigned char i, unsigned char c)
{
    if (i != 0xFF)
        ColorRect(pl >> 3, (pr + 1 - pl) >> 3, (pt >> 3) + 2 * i, 2, c);
}

static unsigned int mx, my;
static void look(void)
{
    static unsigned char i;
    do {
        mx = mouseXPos;
        my = mouseYPosW;
    } while (mx != mouseXPos || my != mouseYPosW);
    i = mx >= pl && mx <= pr && my >= pt && my <= pb ? (unsigned char)((my - pt) >> 4) : 0xFF;
    if (i != ph) {
        plight(ph, 0);
        plight(i, C_THUMB);
        ph = i;
    }
    cw_idle = 1;                        /* (the gate's player goes on) */
}

static unsigned char down(void)
{
    return !(mouseData & 0x80);
}

/* t_arg 0: the cursor's word, 1: the word at r11, r12 */
void sp_pop(void)
{
    static pos_t p;
    static unsigned int L, w, y;
    static unsigned char i, r, k, n, got;
    p = cur;
    if (t_arg) {
        r = view_row(r12);
        if (r12 < TA_TOP || r12 > TA_BOT || r == 0xFF || r11 < PG_L || r11 > PG_R)
            return;
        p = line_hit(top_line + r, r11);
    }
    lt_find(p);
    L = lf_i;
    if (L < top_line || L >= top_line + vn)
        return;
    i = (unsigned char)(L - top_line);
    sp_tokat(lf_pos, lf_pos + vr[i].l.len);   /* (the row's entry: main RAM; lt_get maps its bank over this overlay) */
    got = 0;
    while (sp_tok())
        if (sp_ws <= p && p <= sp_we) {
            got = 1;
            break;
        }
    if (!got) {
        set_status("No word here");
        return;
    }
    wdl = sp_wl;
    memcpy(wd, sp_word, wdl);
    wd[wdl] = 0;
    fws = sp_ws;
    fwe = sp_we;
    SetPointer(PTR_BUSY);
    sp_drop(2);                         /* (W61: asked for: a dictionary that came since is found now) */
    r = sp_check();
    sp_end();
    if (r != SP_UNKNOWN || (wdl <= 5 && wcase() == 2)) {   /* (an abbreviation: spell.c's sp_abbr) */
        SetPointer(PTR_ARROW);
        set_status(r == SP_UNSURE ? "No dictionary: Tools > Get dictionaries" : "The word is in the dictionary");
        return;
    }
    if (t_arg) {
        anc = fws;                      /* (the word clicked: selected) */
        cur = fwe;
        ui_moved();
        view_refresh();
    }
    word_again();                       /* (the drawing's scan used sp_lw) */
    suggestions();
    SetPointer(PTR_ARROW);
    word_again();
    if (ns > 5)
        ns = 5;
    pn = ns + 2;
    /* where: under the word's line, else over it; the word's x */
    i = (unsigned char)(L - top_line);
    pl = line_x(L, fws) & ~7;
    gs_font();
    for (w = uw(pitem[0]), k = 0; k < ns; ++k)
        if ((n = gs ? gw(sg[k]) : uw(sg[k])) > w)
            w = n;
    w = (w + 16 + 7) & ~7;
    if (pl + w > PG_R + 1)
        pl = (PG_R + 1 - w) & ~7;
    if (pl < PG_L)
        pl = PG_L;
    pr = pl + w - 1;
    y = vr[i].y + vr[i].l.h;
    pt = (y + 7) & ~7;
    if (pt + (pn << 4) > ta_bot + 1)
        pt = (vr[i].y - (pn << 4)) & ~7;
    if (pt < TA_TOP)
        pt = TA_TOP;
    pb = pt + (pn << 4) - 1;
    view_cursor(0);
    /* drawn on the front screen */
    ColorRect(pl >> 3, w >> 3, pt >> 3, pn << 1, 0);
    dispBufferOn = ST_WR_FORE;
    SetPattern(0);
    for (k = 0; k < ns; ++k) {
        WRectangle(pl, pt + (k << 4), pr, pt + (k << 4) + 15);
        if (gs) {
            band(pt + (k << 4));
            put(sg[k], pl + 8);
            front(pt + (k << 4) + 1);
        } else
            label(sg[k], pl + 8, pt + (k << 4) + 11);
    }
    WRectangle(pl, pt + (ns << 4), pr, pb);
    UseFont(FONT_UI);
    currentMode = 0;
    for (k = 0; k < 2; ++k) {
        y = pt + ((ns + k) << 4);
        if (!k && ns)
            WHorizontalLine(0x55, pl + 1, pr - 1, y);
        label(pitem[k], pl + 8, y + 11);
    }
    UseFont(0);
    WFrameRectangle(0xFF, pl, pt, pr, pb);
    /* chosen as popup.c's lists are: a press, a drag, a release on an
       item; or a click and a click; Esc or a click outside: none */
    ph = 0xFF;
    r = 0xFF;
    while (down())
        look();
    for (;;) {
        look();
        k = GetNextChar();
        if (!k) {
            k = sp_getin();             /* (the ROM's codes: Esc, Return, digits; down $11, up $91) */
            if (k == 0x91)
                k = KEY_UP;
        }
        if (k == KEY_ESC)
            break;
        if (k == KEY_DOWN || k == KEY_UP) {
            plight(ph, 0);
            ph = ph == 0xFF ? 0 : k == KEY_DOWN ? (ph + 1) % pn : (ph + pn - 1) % pn;
            plight(ph, C_THUMB);
            continue;
        }
        if (k == CR && ph != 0xFF) {
            r = ph;
            break;
        }
        if (k >= '1' && k < '1' + pn) {
            r = k - '1';
            break;
        }
        if (down()) {
            while (down())
                look();
            look();
            r = ph;
            break;
        }
    }
    ColorRect(pl >> 3, w >> 3, pt >> 3, pn << 1, 0);
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    WRecoverRectangle(pl, pt, pr, pb);
    if (r == 0xFF)
        return;
    if (r < ns) {
        strcpy(chg, sg[r]);
        ul_step(U_CMD, cur, anc);
        cur = anc = change_at(fws, fwe);
        ui_moved();
    } else if (r == ns) {
        if (sp_adduser())
            message("The word could not be added", "(the user's words full, or the card).");
        view_all();
    } else {
        sp_ignore();
        view_all();
    }
    need_view = 1;
}

/* (ui.c's) F7: Check spelling; F8: the cursor's word's suggestions (t_arg the key) */
void sp_fkey(void)
{
    if (t_arg == KEY_F8) {
        t_arg = 0;
        sp_pop();
    } else
        callb(bank25, spp_open);
    t_arg = 0;
}

/* (ui.c's tick, the main loop's idle pass) a right click on a word: its
   suggestions; live checking when no key came for 8 frames (t_arg 1:
   something done) */
static unsigned char rdown = 0;
void sp_tick(void)
{
    static unsigned char b;
    b = mouseButtons & 2;
    t_arg = 0;
    if (b && !rdown) {
        r11 = mouseXPos;
        r12 = mouseYPosW;
        t_arg = 1;
        sp_pop();
        t_arg = 1;
    } else if (sp_pend && (unsigned char)(cw_jiffies() - sp_kj) >= 8)
        t_arg = sp_idle();
    rdown = b;
}

#ifdef CW_TEST
/*---------------------------------------------------------------
  the gate's measurements (test.c: 'L' $7D-$7F, t_arg): "CW SPB name
  cycles" for lookups of a word: from the card (the cache empty, the
  file closed), from the cache (the hash emptied), from the hash; a
  word not in the dictionary; its suggestions; in both languages.
  $7E: spelling as Writer did it (no hash, two banks of cache; live
  marking off meanwhile); $7D: as Write does it, the cache and the hash
  emptied (a cold start)
 ---------------------------------------------------------------*/
static unsigned long clk(void)
{
    static unsigned long c;
    ((unsigned char *)&c)[0] = *(volatile unsigned char *)0x9FB8;
    ((unsigned char *)&c)[1] = *(volatile unsigned char *)0x9FB9;
    ((unsigned char *)&c)[2] = *(volatile unsigned char *)0x9FBA;
    ((unsigned char *)&c)[3] = *(volatile unsigned char *)0x9FBB;
    return c;
}

static unsigned long b0;
static void __fastcall__ blog(const char *name, unsigned char n)
{
    static const char h[] = "0123456789ABCDEF";
    static unsigned long d;
    static char *p;
    static signed char i;
    d = clk() - b0;
    p = (char *)iobuf;
    strcpy(p, "CW SPB ");
    strcat(p, name);
    p += strlen(p);
    *p++ = ' ';
    for (i = 28; i >= 0; i -= 4)
        *p++ = h[(d >> i) & 15];
    *p++ = ' ';
    *p++ = h[n >> 4];
    *p++ = h[n & 15];
    *p = 0;
    LogText(iobuf);
}

static void __fastcall__ word(const char *w)
{
    sp_wl = (unsigned char)strlen(w);
    memcpy(sp_lw, w, sp_wl);
}

static void __fastcall__ bench1(unsigned char lang, const char *known, const char *other, const char *bad)
{
    static unsigned char r;
    fmt_lang = lang;
    sp_aside();                         /* (the cache's banks back: empty when taken again) */
    b0 = clk();
    r = sp_ready();
    blog(lang == LANG_PL ? "pl-load" : "en-load", r);
    sp_end();
    sp_drop(0);
    word(known);
    b0 = clk();
    r = sp_check();
    blog("cold", r);
    sp_end();
    sp_drop(0);
    word(known);
    b0 = clk();
    r = sp_check();
    blog("cached", r);
    word(known);
    b0 = clk();
    r = sp_check();
    blog("hash", r);
    word(other);
    b0 = clk();
    r = sp_check();
    blog("cold2", r);
    sp_end();
    word(bad);
    b0 = clk();
    r = sp_check();
    blog("unknown", r);
    sp_end();
    word(bad);
    b0 = clk();
    sp_suggest();
    bk_rd(sp_xb, 0xA000u + SP_SUGG, sp_buf, 1);
    blog("sugg", sp_buf[0]);
}

void sp_bench(void)
{
    static unsigned char l;
    l = fmt_lang;
    if (t_arg == 0x7F) {
        /* (komputer, źródło, komputre; receive, zymurgy, recieve) */
        bench1(LANG_PL, "komputer", "\xAE" "r\xF3" "d\xB3" "o", "komputre");
        bench1(LANG_EN, "receive", "zymurgy", "recieve");
        fmt_lang = l;
        sp_ready();
        sp_end();
    } else {
        sp_tflags = t_arg == 0x7E ? 3 : 0;
        sp_live = t_arg != 0x7E;
        sp_aside();                     /* (the cache as the mode has it, empty) */
        sp_drop(0);
    }
    t_arg = 0;
}
#endif

/*---------------------------------------------------------------
  (W61) the dictionaries are not in Veranda's package: Tools > Get
  dictionaries has Veranda Update fetch them (Veranda's DECISIONS V198):
  two requests in /VERANDA/.SYSTEM, START.REQ (the desktop starts the
  program it names when it next starts) and UPDATE.REQ ("dict": Veranda
  Update checks the dictionaries' list at once, and asks which to get;
  "back Veranda Write": its Quit brings Write back), then Write puts
  itself aside (as Alt+Tab does). When it is back, Check spelling (F7)
  finds them (sp_drop(2): a dictionary not there is looked for again)
 ---------------------------------------------------------------*/
static void __fastcall__ nodict(unsigned char k)
{
    /* (a box's line: 28 characters at most; F7's panel says the rest) */
    message(k == LANG_PL ? "No Polish dictionary:" : "No English dictionary:", "Tools > Get dictionaries.");
}

#define RQ ((char *)iobuf + IOBUF - 64)  /* (main RAM: the kernel reads it) */

/* text into the file RQ names (in the folder made current): 0 done */
static unsigned char __fastcall__ rq_put(const char *text)
{
    static unsigned char h, e;
    h = FOpen(RQ, 'w');
    if (h < 2)
        return 1;
    strcpy((char *)iobuf, text);
    e = FWrite(h, iobuf, strlen((char *)iobuf));
    e |= FClose(h);
    return e;
}

#define SP ((char *)iobuf + IOBUF - 128) /* (Veranda Update's folder, then START.REQ's text) */

static void getdict(void)
{
    static unsigned char h, e;
    /* Veranda Update, in /VERANDA/APPS (or where the applications are) */
    strcpy(SP, vr_cd(VR_APPS));
    strcpy(RQ, "Veranda Update.cvt");
    h = FOpen(RQ, 'r');
    if (h >= 2)
        FClose(h);
    vr_cdback();
    if (h < 2) {
        message("No Veranda Update: copy the", ".DIC files to /VERANDA/DICT.");
        return;
    }
    if (!yesno("Get the dictionaries? Veranda", "Update fetches them (Wi-Fi)."))
        return;
    if (SP[1])
        strcat(SP, "/");
    strcat(SP, "Veranda Update\r\n");
    vr_cd(VR_DATA);
    strcpy(RQ, "UPDATE.REQ");
    e = rq_put("dict\r\nback Veranda Write\r\n");
    strcpy(RQ, "START.REQ");
    if (!e)
        e = rq_put(SP);
    vr_cdback();
    if (e) {
        message("The card could not be", "written (/VERANDA/.SYSTEM).");
        return;
    }
    sp_drop(2);
    set_status("Veranda Update gets the dictionaries");
    TaskCtl(TASK_ASIDE, 0, 0);
}

/*---------------------------------------------------------------
  the Tools menu (t_arg: its item)
 ---------------------------------------------------------------*/
void t_tools(void)
{
    static unsigned char k;
    k = t_arg;
    if (spp_on == 2)
        callb(bank24, so_close);        /* (W60: the options' panel gives way) */
    if (k == 5) {
        callb(bank24, so_open);         /* (W60: Spelling options) */
        return;
    }
    if (k == 6) {
        getdict();                      /* (W61: Get dictionaries) */
        return;
    }
    if (k == 0) {
        callb(bank25, spp_open);
        return;
    }
    if (k == 1) {
        t_arg = 0;
        sp_pop();
        return;
    }
    if (k == 2) {
        /* (W60: kept in the settings at once; on: as you work too) */
        sp_cfg = sp_live ? sp_cfg | SPC_NOMARK : sp_cfg & ~(SPC_NOMARK | SPC_ASK);
        callb(bank24, so_save);
        set_status(sp_live ? "Misspellings marked as you type" : "Misspellings not marked");
        if (sp_live) {                  /* (W61: turned on without the dictionary: said once, here) */
            sp_drop(2);
            if (!sp_ready())
                nodict(fmt_lang);
            sp_end();
        }
    } else {
        k = k == 3 ? LANG_EN : LANG_PL;
        if (k == fmt_lang)
            return;
        fmt_lang = k;
        doc_modified = 1;               /* (the language is kept in the document) */
        SetPointer(PTR_BUSY);
        sp_drop(2);                     /* (W61) */
        if (!sp_ready())
            nodict(k);
        sp_end();
        SetPointer(PTR_ARROW);
        set_status(k == LANG_PL ? "Spelling: Polish" : "Spelling: English");
        if (spp_on == 1)
            callb(bank25, spp_open);    /* (the panel's word: the new language's) */
    }
    view_all();
    need_view = status_dirty = 1;
}
