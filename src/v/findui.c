/* Veranda Write on Veranda: the Find and Replace panel (overlay 9).

   Ctrl+F (Find), Ctrl+H or Ctrl+R (Replace) open it under the text (the
   view's bottom ta_bot moves up to FP_Y - 1): two fields, Find and
   Replace with, Match case and Whole word, the buttons Find next,
   Replace and Replace all, a line for what happened. While it is open the
   typing goes to its field (Tab: the other one; Backspace; Return: Find
   next; Esc: closed), Alt+C and Alt+W the options, Alt+N, Alt+R, Alt+A
   the buttons; the other commands and the arrows go to the document.
   Ctrl+G finds the next match with the panel open or not. find.c (overlay
   4) does the finding; a match found is selected and brought into view.
   Replace all is one undo step.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#pragma code-name ("OVERLAY9")
#pragma rodata-name ("OVL9RO")

unsigned char fp_on, fp_focus, fp_flags;
char f_find[FD_MAX + 1], f_repl[FD_MAX + 1];
static char fl[48] = { 0 };             /* (DATA, main RAM: what the kernel draws) */

/* (Veranda Look 3, Sunroom) the panel on the chrome under its top line;
   the fields white on whole cards, a frame with cut corners inside them;
   the buttons the same, Find next (Return's) with a double frame; their
   card rows: */
#define FX     112                      /* the fields: x, width (cards 14-42) */
#define FW     232
#define FY0    (FP_Y + 8)               /* Find's and the options' (rows 51-52), */
#define FY1    (FP_Y + 32)              /* Replace's and the buttons' (54-55), */
#define FYM    (FP_Y + 59)              /* the message's baseline (56-57) */
#define BX     360                      /* the options and buttons */

static void text(const char *s, unsigned int x, unsigned int y)
{
    strncpy(fl, s, sizeof fl - 1);
    WPutString(fl, x, y);
}

/* field i (0 Find, 1 Replace): its frame, its text, the cursor if it has
   the typing */
static void field(unsigned char i)
{
    unsigned int y = i ? FY1 : FY0, w = 0;
    const char *t = i ? f_repl : f_find, *e;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    UseFont(FONT_UI);
    currentMode = 0;
    SetPattern(0);
    WRectangle(FX, y, FX + FW - 1, y + 15);
    for (e = t; *e; ++e)
        w += GetCharWidth(*e);
    while (w > FW - 14)                 /* (a long text: its end) */
        w -= GetCharWidth(*t++);
    text(t, FX + 5, y + 11);
    rbox(FX + 1, y + 1, FX + FW - 2, y + 14);   /* (after the text: it clears its lines) */
    if (fp_focus == i)
        WInvertRectangle(FX + 6 + w, y + 3, FX + 7 + w, y + 12);
    UseFont(0);
}

static void box(unsigned int x, const char *s, unsigned char on)
{
    text(s, x + 14, FY0 + 11);
    rbox(x, FY0 + 3, x + 9, FY0 + 12);
    if (on)
        WInvertRectangle(x + 2, FY0 + 5, x + 7, FY0 + 10);
}

static void options(void)
{
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    UseFont(FONT_UI);
    currentMode = 0;
    SetPattern(0);
    WRectangle(BX, FY0, PG_R - 2, FY0 + 15);
    box(BX, "Match case", fp_flags & FF_CASE);
    box(BX + 112, "Whole word", fp_flags & FF_WORD);
    UseFont(0);
}

static void say(const char *s)
{
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    UseFont(FONT_UI);
    currentMode = 0;
    SetPattern(0);
    WRectangle(PG_L, FYM - 11, PG_R, TA_END);
    text(s, FX + 5, FYM);
    UseFont(0);
}

static const char *const bname[3] = { "Find next", "Replace", "Replace all" };
static const unsigned int bx0[4] = { BX, BX + 80, BX + 152, PG_R + 1 };  /* (cards; 8 pixels between) */

static void draw(void)
{
    unsigned char i;
    unsigned int x, w;
    const char *s;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(PG_L, FP_Y, PG_R, TA_END);
    WHorizontalLine(0xFF, PG_L, PG_R, FP_Y);
    ColorRect(PG_C0, PG_CW, FP_Y >> 3, (TA_END + 1 - FP_Y) >> 3, C_BAR);
    ColorRect(FX >> 3, FW >> 3, FY0 >> 3, 2, 0);
    ColorRect(FX >> 3, FW >> 3, FY1 >> 3, 2, 0);
    UseFont(FONT_UI);
    currentMode = 0;
    text("Find", PG_L + 10, FY0 + 11);
    text("Replace", PG_L + 10, FY1 + 11);
    for (i = 0; i < 3; ++i) {
        x = bx0[i];
        w = bx0[i + 1] - 8 - x;
        ColorRect(x >> 3, w >> 3, FY1 >> 3, 2, 0);
        s = bname[i];
        x += w >> 1;
        while (*s)
            x -= GetCharWidth(*s++) >> 1;
        text(bname[i], x, FY1 + 11);
        x = bx0[i];
        rbox(x + 1, FY1 + 1, x + w - 2, FY1 + 14);
        if (!i)                         /* (the default: a double frame) */
            rbox(x + 2, FY1 + 2, x + w - 3, FY1 + 13);
    }
    UseFont(0);
    field(0);
    field(1);
    options();
    say("Return: find next   Tab: the other field   Esc: close");
}

void fp_open(void)
{
    view_cursor(0);
    fp_focus = t_arg;
    if (!fp_on) {
        fp_on = 1;
        ta_bot = FP_Y - 1;
        draw();
        view_all();
        follow = 1;
        need_view = 1;
        return;
    }
    field(0);
    field(1);
}

static void close(void)
{
    fp_on = 0;
    ta_bot = TA_END;
    ColorRect(PG_C0, PG_CW, FP_Y >> 3, (TA_END + 1 - FP_Y) >> 3, 0);    /* (the page's paper) */
    view_all();
    need_view = 1;
}

/* the next match after the selection (from the start again at the end) */
static unsigned char next(void)
{
    pos_t from = HAS_SEL() ? SEL_HI() : cur;
    if (!f_find[0]) {
        t_arg = 0;
        fp_open();
        return 0;
    }
    SetPointer(PTR_BUSY);
    find_set(f_find, fp_flags);
    if (!find_from(from, DOC_LEN) && !(from && find_from(0, from))) {
        if (fp_on)
            say("Not found.");
        else
            set_status("Not found");
        return 0;
    }
    anc = fd_lo;
    cur = pos_norm(fd_hi);
    ui_moved();
    if (fp_on)
        say(fd_lo < from ? "Found (from the start)." : "Found.");
    return 1;
}

void fp_next(void)
{
    next();
}

static void replace(void)
{
    pos_t lo = SEL_LO(), hi = SEL_HI();
    unsigned char t;
    find_set(f_find, fp_flags);
    if (HAS_SEL() && find_here(lo, hi)) {
        ul_step(U_CMD, cur, anc);
        t = style_at(lo);
        cur = anc = lo = doc_delete(lo, hi);
        if (f_repl[0])
            cur = anc = doc_insert(lo, (unsigned char *)f_repl, strlen(f_repl), t);
        ui_moved();
    }
    next();
}

static void replace_every(void)
{
    static char m[48];
    unsigned int n;
    unsigned char k = 0, d[6];
    if (!f_find[0])
        return;
    find_set(f_find, fp_flags);
    SetPointer(PTR_BUSY);
    lay_busy = busy_ptr;
    ul_step(U_CMD, cur, anc);
    tk_a = cur;
    tk_b = anc;
    n = replace_all((unsigned char *)f_repl, strlen(f_repl));
    lay_busy = 0;
    cur = pos_norm(tk_a);
    anc = pos_norm(tk_b);
    if (cur > DOC_LEN || anc > DOC_LEN)
        cur = anc = pos_norm(0);
    ui_moved();
    follow = 0;
    view_all();
    do {
        d[k++] = (unsigned char)('0' + n % 10);
        n /= 10;
    } while (n);
    n = 0;
    while (k)
        m[n++] = d[--k];
    strcpy(m + n, doc_err ? " replaced (no room for more)." : ul_lost ? " replaced (too many to undo)."
                  : " replaced (one undo step).");
    say(m);
    doc_err = 0;
}

void fp_key(void)
{
    unsigned char k = keyData, m = keyDataMods, i;
    char *s = fp_focus ? f_repl : f_find;
    t_arg = 1;
    if (m & 4) {                        /* Alt + a letter: the options, the buttons */
        switch (k & 0x5F) {
        case 'C': fp_flags ^= FF_CASE; options(); return;
        case 'W': fp_flags ^= FF_WORD; options(); return;
        case 'N': next(); return;
        case 'R': replace(); return;
        case 'A': replace_every(); return;
        }
    }
    if (m & (2 | KEYM_ARROW) || (m & 4)) {
        t_arg = 0;                      /* (the commands, the arrows: the document's) */
        return;
    }
    i = strlen(s);
    switch (k) {
    case KEY_ESC:
        close();
        return;
    case KEY_TAB:
        fp_focus ^= 1;
        field(0);
        field(1);
        return;
    case CR:
    case 0x8D:
        next();
        return;
    case 8:
    case KEY_DELETE:
        if (i) {
            s[i - 1] = 0;
            field(fp_focus);
        }
        return;
    case 0xA0:
        k = ' ';
        break;
    }
    if (((k >= 32 && k < 127) || k >= 160) && i < FD_MAX) {
        s[i] = k;
        s[i + 1] = 0;
        field(fp_focus);
        return;
    }
    t_arg = 0;
}

void fp_press(void)
{
    unsigned int x = r11, y = r12;
    unsigned char i;
    if (y >= FY0 && y < FY1 + 16 && x >= FX && x < FX + FW) {
        fp_focus = y >= FY1 - 4;
        field(0);
        field(1);
    } else if (y >= FY0 && y < FY0 + 16 && x >= BX) {
        fp_flags ^= x < BX + 112 ? FF_CASE : FF_WORD;
        options();
    } else if (y >= FY1 && y < FY1 + 16)
        for (i = 0; i < 3; ++i)
            if (x >= bx0[i] && x < bx0[i + 1] - 8) {
                if (i == 0)
                    next();
                else if (i == 1)
                    replace();
                else
                    replace_every();
            }
}

/*---------------------------------------------------------------
  Word count, Go to page, Insert date and time
 ---------------------------------------------------------------*/
/* v with a comma every three digits at p; the end */
static char *num(char *p, unsigned long v)
{
    char b[14];
    unsigned char n = 0;
    do {
        if (n == 3 || n == 7 || n == 11)
            b[n++] = ',';
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *p++ = b[--n];
    *p = 0;
    return p;
}

static char *str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    *p = 0;
    return p;
}

/* the pages (pages.c, as the status bar's): the number of pages, and
   the first line of page pg (pg_line); all of the document read first */
static unsigned int pages(unsigned int pg)
{
    if (ld_kind)
        ld_need(0xFFFFFFFFUL);
    pg_arg = pg ? pg : 1;
    pg_first();
    return pg_tot;
}

/* a box of four lines (r5-r8) and OK, its table in iobuf (main RAM) */
static const unsigned char countbox[] = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0E,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x10,
    DBVARSTR, TXT_LN_X, TXT_LN_4_Y, 0x12,
    OK, DBI_X_2, DBI_Y_2,
    0
};

void t_count(void)
{
    char *p;
    unsigned long w;
    SetPointer(PTR_BUSY);
    w = 0;
    if (HAS_SEL()) {
        doc_count(SEL_LO(), SEL_HI());
        w = cn_words;
    }
    doc_count(0, DOC_LEN);
    p = str(msg_a, "Words: ");
    p = num(p, cn_words);
    if (HAS_SEL()) {
        p = str(p, " (");
        p = num(p, w);
        str(p, " selected)");
    }
    p = str(msg_b, "Characters: ");
    num(p, cn_chars);
    p = str(itext[0], "Without spaces: ");
    num(p, cn_nonsp);
    p = str(itext[1], "Paragraphs: ");
    p = num(p, cn_paras);
    p = str(p, "   Pages: ");
    num(p, pages(0));
    memcpy(iobuf, countbox, sizeof countbox);
    r5 = (unsigned int)msg_a;
    r6 = (unsigned int)msg_b;
    r7 = (unsigned int)itext[0];
    r8 = (unsigned int)itext[1];
    view_cursor(0);
    DoDlgBox(iobuf);
    need_view = 1;
    status_dirty = 1;
}

/* Go to page: a box with a field (r5 its text, itext[2]) */
static const unsigned char gotobox[] = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0E,
    DBGETSTRING, TXT_LN_X, TXT_LN_2_Y + 4, 0x0C, 5,
    OK, DBI_X_0, DBI_Y_2,
    CANCEL, DBI_X_2, DBI_Y_2,
    0
};

void t_goto(void)
{
    char *p;
    unsigned int n = pages(0), pg = 0;
    p = str(msg_b, "Go to page (1 - ");
    p = num(p, n);
    str(p, "):");
    itext[2][0] = 0;
    memcpy(iobuf, gotobox, sizeof gotobox);
    r5 = (unsigned int)itext[2];
    r6 = (unsigned int)msg_b;
    view_cursor(0);
    if (DoDlgBox(iobuf) == OK) {
        for (p = itext[2]; *p >= '0' && *p <= '9'; ++p)
            pg = pg * 10 + (*p - '0');
        if (pg < 1)
            pg = 1;
        if (pg > n)
            pg = n;
        pages(pg);
        cur = anc = pos_norm(lt_start(pg_line));
        ui_moved();
    }
    need_view = 1;
    status_dirty = 1;
}

static const char *const mname[12] = {
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
    "November", "December"
};

/* "21 November 2026, 14:05" typed at the cursor */
void t_date(void)
{
    static char d[32];
    char *p = num(d, day);
    *p++ = ' ';
    p = str(p, mname[(month - 1) % 12]);
    *p++ = ' ';
    p = num(p, 2000 + year);
    p = str(p, ", ");
    *p++ = (char)('0' + hour / 10);
    *p++ = (char)('0' + hour % 10);
    *p++ = ':';
    *p++ = (char)('0' + minutes / 10);
    *p++ = (char)('0' + minutes % 10);
    *p = 0;
    ul_step(U_CMD, cur, anc);
    if (HAS_SEL()) {
        ty_style = style_typing(SEL_LO());
        cur = anc = doc_delete(SEL_LO(), SEL_HI());
    }
    cur = anc = doc_insert(cur, (unsigned char *)d, strlen(d), ty_style);
    ul_break();
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

/*---------------------------------------------------------------
  paragraph styles: the selection's paragraphs (or the cursor's)
 ---------------------------------------------------------------*/
static void par_done(void)
{
    cur = pos_norm(tk_a);
    anc = pos_norm(tk_b);
    if (doc_err == DE_PAR) {
        message("The document has too many paragraph styles", "(32 at most).");
        doc_err = 0;
    } else if (doc_err) {
        message("There is not enough memory", "for that.");
        doc_err = 0;
    }
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void __fastcall__ para(unsigned char op, unsigned char v)
{
    ul_step(U_CMD, cur, anc);
    tk_a = cur;
    tk_b = anc;
    doc_para(SEL_LO(), SEL_HI(), op, v);
    par_done();
}

/* the named styles' characters (edit.c's rule): for the typing style */
static const unsigned char nm_size[PK_N] = { 0, 27, 18, 12, 0, 0, 0 };
static const unsigned char nm_flags[PK_N] = { 0, SF_BOLD, SF_BOLD, SF_BOLD, SF_ITALIC, 0, 0 };

static void __fastcall__ set_named(unsigned char k)
{
    unsigned char o = par[par_at(par_start(SEL_LO()))].kind, t, kept = ty_kept && !HAS_SEL();
    ul_step(U_CMD, cur, anc);
    tk_a = cur;
    tk_b = anc;
    doc_named(SEL_LO(), SEL_HI(), k);
    par_done();
    /* what is typed next (an empty paragraph has no characters to take it;
       a typing style chosen is kept) */
    t = kept ? ty_style : style_typing(SEL_LO());
    if (nm_flags[o])
        t = fmt_map(t, FMT_CLR, nm_flags[o]);
    if (IS_HEAD(k))
        t = fmt_map(t, FMT_SIZE, nm_size[k]);
    else if (IS_HEAD(o) && t != 0xFF)
        t = fmt_map(t, FMT_SIZE, DEF_SIZE);
    if (nm_flags[k] && t != 0xFF)
        t = fmt_map(t, FMT_SET, nm_flags[k]);
    if (t != 0xFF) {
        ty_style = t;
        ty_kept = !HAS_SEL();
    }
}

/* the Format menu's items */
static void __fastcall__ fmt_par(unsigned char i)
{
    static const unsigned char op[10] = { PF_ALIGN, PF_ALIGN, PF_ALIGN, PF_ALIGN, PF_INDENT, PF_INDENT,
                                          PF_SPACING, PF_SPACING, PF_SPACING, PF_AFTER };
    static const unsigned char v[10] = { PA_LEFT, PA_CENTRE, PA_RIGHT, PA_JUSTIFY, 20, (unsigned char)-20, 2, 3, 4, 8 };
    unsigned char a = v[i];
    if (i == 9 && par[par_at(par_start(SEL_LO()))].after)
        a = 0;                          /* (Space after: on and off) */
    para(op[i], a);
}

/* after Return at a heading's end (kind t_arg): Body's characters */
void t_body(void)
{
    unsigned char t = fmt_map(ty_style, FMT_CLR, nm_flags[t_arg]);
    if (t != 0xFF)
        t = fmt_map(t, FMT_SIZE, DEF_SIZE);
    if (t != 0xFF) {
        ty_style = t;
        ty_kept = 1;
    }
}

void t_named(void)
{
    set_named(t_arg);
}

void t_format(void)
{
    fmt_par(t_arg);
}


