/* Veranda Write on Veranda: the Table menu (the bar's last), Insert >
   Table, its boxes, a page break refused in a table, About; templates
   (overlay 29; DECISIONS W62-W67). The table edits are tabedit.c's
   (overlay 28: through glue.s; the row's cells, flags and widths, tr_n,
   tr_f, tr_w, and the cursor's place after an edit, tb_at, in main RAM).

   Templates (W67): File > New from template lists Veranda's (VR_SYS's
   TEMPLATES/EN, or TEMPLATES/PL when the documents' language is Polish,
   Tools > Spelling options) and the user's own (/DOCUMENTS/Templates)
   with a blank document first; the chosen one opens as Untitled (the
   template's file is only read), "{date}" in it today's date (in Polish
   in a Polish one). File > Save as template saves into
   /DOCUMENTS/Templates (made when missing: Veranda's updates never
   write there).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"
#pragma code-name ("OVERLAY29")
#pragma rodata-name ("OVL29RO")
#pragma bss-name ("OVL29BSS")

extern char pdf_name[64];               /* (main.c: main RAM, a file's name) */
extern unsigned char lb[128];           /* (view.c: main RAM, cb_read's) */
extern char sl[64], sr[40];             /* (view.c: main RAM, the status bar's) */
unsigned char cw_esc(void);             /* (glue.s) */
void pg_export(void);                   /* (pgview.c, overlay 14: today's date into iobuf) */

/* (tools.c's, overlay 8, moved here for its room) the menu bar's layout:
   strw as tools.c's (a group's mark not drawn, a tab 24 pixels) */
static unsigned int __fastcall__ mstrw(const char *s)
{
    unsigned int w = 0;
    for (; *s; ++s)
        if (*s == '\t')
            w += 24;
        else if (*s != '\x1f')
            w += GetCharWidth(*s);
    return w;
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
        x += (i ? 12 : 30) + mstrw(mbar.i[i].text) + (i ? 0 : strlen(mbar.i[i].text));
        x = (x + 7) & ~7;
    }
    mbar.right = x + 64;
    UseSystemFont();
}

/* tabedit.c's edit at the cursor (tbui.c's tb_do, overlay 28): op, tr_w
   its numbers (tr_n of them); its result */
static unsigned char FASTCALL edit(unsigned char op)
{
    t_arg = op;
    callb(bank28, tb_do);
    return t_arg;
}
#define in_row() edit(TD_ROW)

/* (ui.c's ui_main, moved here for its room) the start: t_arg the
   shell's options */
void tb_start(void)
{
    static unsigned char opts;
    opts = t_arg;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_home(opts);                      /* /DOCUMENTS first (Veranda's DECISIONS V71) */
    if (curDirName[0] != '/')
        strcpy((char *)curDirName, "/");
    longNames = LONG_NAMES;
    sw_init();                          /* the paged cbufs (the text, the line table) */
    bk_keep = 2;                        /* (banks left to Veranda: accessories, the kernel) */
    ul_init();
    callb(bank14, pg_loaddef);          /* (the user's page setup for new documents, W44) */
    doc_new();
    ty_style = 0;
    menus_layout();
    view_frame();
#ifdef CW_TEST
    test_start();
#endif
    sw_start();                         /* the swap's folder (a stale one removed) */
    if ((opts & ST_LD_DATA) && dataFileName[0])
        CALL2(f_startdoc);
}

/* (pdf.c's, overlay 15's room) the PDF encoding object's text, part k,
   in iobuf: its bytes */
#include "../pdfenc.h"

unsigned int __fastcall__ e_pdf_enc(unsigned char k)
{
    static const char a[] = PDF_ENC0, b[] = PDF_DIFF PDF_ENC1;
    memcpy(iobuf, k ? b : a, k ? sizeof b - 1 : sizeof a - 1);
    return k ? sizeof b - 1 : sizeof a - 1;
}

static void FASTCALL go(pos_t p)
{
    cur = anc = pos_norm(p);
    ty_kept = 0;
    goal_x = 0xFFFF;
    ui_moved();
}

static void after(void)
{
    ul_break();
    go(tb_at);
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void FASTCALL said(unsigned char e)
{
    if (e == DE_MEM)
        message("There is not enough memory", "for that.");
    doc_err = 0;
}

/*---------------------------------------------------------------
  the Table menu (the bar's item 10), Insert > Table
 ---------------------------------------------------------------*/
static const char *const tmn[] = {
    "Insert table...", "\x1fInsert row above", "Insert row below", "Insert column left", "Insert column right",
    "\x1f" "Delete row", "Delete column", "Delete table", "\x1f" "Column widths...", "Equal column widths",
    "\x1f" "Borders"
};
#define NTM 11

void tb_mtext(void)
{
    static unsigned char i;
    for (i = 0; i < NTM; ++i)
        strcpy(itext[i], tmn[i]);
    if (in_row())
        strcat(itext[NTM - 1], tr_f & TR_NOGRID ? "\toff" : "\ton");
    t_arg = NTM;
}

/* a box with a line and a field (itext[2], n characters) */
static const unsigned char fbox[] = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0E,
    DBGETSTRING, TXT_LN_X, TXT_LN_2_Y + 4, 0x0C, 24,
    OK, DBI_X_0, DBI_Y_2,
    CANCEL, DBI_X_2, DBI_Y_2,
    0
};

static unsigned char FASTCALL ask(const char *q)
{
    strcpy(msg_b, q);
    memcpy(iobuf, fbox, sizeof fbox);
    r5 = (unsigned int)itext[2];
    r6 = (unsigned int)msg_b;
    view_cursor(0);
    need_view = 1;
    status_dirty = 1;
    return DoDlgBox(iobuf) != CANCEL;   /* (Return in the field ends it with DBGETSTRING) */
}

/* the field's numbers (up to n of them, apart by anything else) into nb */
static unsigned char nb[TB_MAXC];

static unsigned char FASTCALL nums(unsigned char n)
{
    static unsigned char k, d;
    static unsigned int v;
    static const char *p;
    for (k = 0, p = itext[2]; *p && k < n;) {
        if (*p < '0' || *p > '9') {
            ++p;
            continue;
        }
        for (v = 0, d = 0; *p >= '0' && *p <= '9'; ++p)
            if (++d < 4)
                v = v * 10 + (*p - '0');
        nb[k++] = v > 255 ? 255 : (unsigned char)v;
    }
    return k;
}

static char *FASTCALL put_n(char *p, unsigned char v)
{
    if (v >= 100)
        *p++ = (char)('0' + v / 100);
    if (v >= 10)
        *p++ = (char)('0' + v / 10 % 10);
    *p++ = (char)('0' + v % 10);
    return p;
}

void tb_tinsert(void)
{
    static unsigned char c, r;
    strcpy(itext[2], "3 x 3");
    if (!ask("A table: columns x rows"))
        return;
    if (nums(2) < 1)
        return;
    c = nb[0];
    r = nb[1];
    if (c < 1)
        c = 1;
    if (c > TB_MAXC)
        c = TB_MAXC;
    if (r < 1)
        r = 1;
    if (r > 99)
        r = 99;
    ul_step(U_CMD, cur, anc);
    if (HAS_SEL()) {
        if (!tb_delok(SEL_LO(), SEL_HI())) {
            set_status("Select in one cell, or all the table");
            return;
        }
        cur = anc = doc_delete(SEL_LO(), SEL_HI());
    }
    tr_w[0] = c;
    tr_w[1] = r;
    r = edit(TD_INSERT);
    if (r == DE_TB) {
        set_status("No table in a table");
        return;
    }
    said(r);
    after();
}

static void widths(void)
{
    static unsigned char k, n, w[TB_MAXC];
    static unsigned int s, r;
    static char *p;
    p = itext[2];
    for (k = 0; k < tr_n; ++k) {
        if (k)
            *p++ = ' ';
        p = put_n(p, (unsigned char)(((unsigned int)tr_w[k] * 100 + 128) >> 8));
    }
    *p = 0;
    if (!ask("Column widths (per cent):"))
        return;
    n = nums(tr_n);
    if (!n)
        return;
    for (k = s = 0; k < n; ++k) {
        w[k] = (unsigned char)(((unsigned int)nb[k] * 256 + 50) / 100);
        if (!w[k])
            w[k] = 1;
        s += w[k];
    }
    /* the columns not given share what is left (at least 5% each); all
       made to fit the text's width */
    if (n < tr_n) {
        r = s + 13 * (tr_n - n) < 256 ? (256 - s) / (tr_n - n) : 13;
        for (; k < tr_n; ++k)
            s += w[k] = (unsigned char)r;
    }
    if (s > 256)
        for (k = 0; k < tr_n; ++k)
            w[k] = (unsigned char)((unsigned int)w[k] * 256 / s);
    memcpy(tr_w, w, tr_n);
    edit(TD_WIDTHS);
}

void tb_cmd(void)
{
    static unsigned char i, e, k, n;
    static unsigned int w;
    i = t_arg;
    if (!i) {
        tb_tinsert();
        return;
    }
    if (!in_row()) {
        set_status("The cursor is not in a table");
        return;
    }
    ul_step(U_CMD, cur, anc);
    tb_at = cur;
    e = 0;
    switch (i) {
    case 8: widths(); break;
    case 9:
        n = tr_n;
        for (k = 0; k < n; ++k) {
            w = 256 / n + (k < 256 % n);
            tr_w[k] = w > 255 ? 255 : (unsigned char)w;
        }
        e = edit(TD_WIDTHS);
        break;
    case 10:
        tr_w[0] = tr_f & TR_NOGRID;
        e = edit(TD_GRID);
        break;
    default:
        e = edit(i);                    /* (rows, columns, the table: TD_ROWUP ... TD_TABLE) */
    }
    if (e == DE_TB)
        set_status("A row has 8 cells at most");
    else
        said(e);
    after();
}

/* Insert > Page break: not in a table (it would end its row) */
void tb_pbreak(void)
{
    if (in_row()) {
        set_status("No page break in a table");
        return;
    }
    callb(bank14, t_pbreak);
}

void tb_about(void)
{
    message("Veranda Write 0.5: a word processor.", "MIT licence, (c) 2026 Marcin Spoczynski");
}

/*---------------------------------------------------------------
  templates (W67)
 ---------------------------------------------------------------*/
#define NTP 12
static char tpn[NTP][40];               /* the names (without .cwr), */
static unsigned char tpk[NTP], ntp;     /*   0 blank, 1 Veranda's, 2 the user's */
static char tdir[2][40];                /* the folders */
static const char *const tsub[2] = { "TEMPLATES/EN", "TEMPLATES/PL" };

static unsigned char polish(void)
{
    if ((sp_cfg & SPC_LANG) == SPC_KEYB) {
        SysPrefs(PREFS_GET, iobuf);
        return (iobuf[6] & PREFS_POLISH) != 0;
    }
    return (sp_cfg & SPC_LANG) == LANG_PL;
}

/* the current folder's .cwr files, kind k */
static void FASTCALL list(unsigned char k)
{
    static unsigned char n, i;
    static char *nm;
    nm = (char *)iobuf + 19;            /* (DirNext's entry at iobuf + 16: its name at + 3) */
    iobuf[0] = '*';
    iobuf[1] = 0;
    if (DirOpen(iobuf))
        return;
    while (!DirNext(iobuf + 16) && ntp < NTP) {
        n = (unsigned char)strlen(nm);
        if (iobuf[16] == 'D' || n < 5 || n > 43 || nm[n - 4] != '.' || (nm[n - 3] | 0x20) != 'c'
            || (nm[n - 2] | 0x20) != 'w' || (nm[n - 1] | 0x20) != 'r')
            continue;
        nm[n - 4] = 0;
        /* (in order of their names) */
        for (i = ntp; i > 1 && tpk[i - 1] == k && strcmp(tpn[i - 1], nm) > 0; --i) {
            strcpy(tpn[i], tpn[i - 1]);
            tpk[i] = k;
        }
        strcpy(tpn[i], nm);
        tpk[i] = k;
        ++ntp;
    }
}

/* the folder of kind k (1 Veranda's, 2 the user's) current, its path in tdir[k - 1]; 0 none */
static unsigned char FASTCALL to_dir(unsigned char k)
{
    static const char *b;
    static char *d;
    d = tdir[k - 1];
    b = vr_cd(k == 1 ? VR_SYS : VR_DOCS);
    strcpy(d, b);
    if (d[1])
        strcat(d, "/");
    strcat(d, k == 1 ? tsub[polish()] : "Templates");
    strcpy((char *)iobuf, d);
    return !ChDir(iobuf);
}

/* the chooser: a list on the front screen (the back one keeps the page:
   drawn again from it after), chosen as the toolbar's pop-ups are (a
   click, Return; Up, Down; Esc or a click outside: none) */
#define CL 192
#define CR_ 448
#define CT 112
static unsigned char hl;
static unsigned int mx, my;

static void FASTCALL light(unsigned char i, unsigned char c)
{
    if (i != 0xFF)
        ColorRect(CL >> 3, (CR_ + 1 - CL) >> 3, (CT >> 3) + 2 + 2 * i, 2, c);
}

static unsigned char FASTCALL item_at(void)
{
    do {
        mx = mouseXPos;
        my = mouseYPosW;
    } while (mx != mouseXPos || my != mouseYPosW);
    if (mx < CL || mx > CR_ || my < CT + 16 || my >= CT + 16 + 16 * ntp)
        return 0xFF;
    return (unsigned char)((my - CT - 16) >> 4);
}

/* a key from the ROM's queue (GETIN: 0 none), which the main loop's
   KeyPoll does not feed while the chooser's loop runs */
static unsigned char gk;

static unsigned char getin(void)
{
    asm("jsr $FFE4");
    asm("sta %v", gk);
    return gk;
}

static unsigned char down(void)
{
    return !(mouseData & 0x80);
}

static unsigned char choose(void)
{
    static unsigned char i, k, r;
    static unsigned int y, b;
    b = CT + 16 + 16 * ntp + 3;
    dispBufferOn = ST_WR_FORE;
    view_cursor(0);
    SetPattern(0);
    WRectangle(CL, CT, CR_, b);
    WFrameRectangle(0xFF, CL, CT, CR_, b);
    WHorizontalLine(0xFF, CL, CR_, CT + 15);
    UseFont(FONT_UI);
    currentMode = SET_BOLD;
    strcpy((char *)iobuf, "New from template");
    WPutString(iobuf, CL + 8, CT + 11);
    currentMode = 0;
    for (i = 0; i < ntp; ++i) {
        y = CT + 16 + 16 * i;
        strcpy((char *)iobuf, tpn[i]);
        if (tpk[i] == 2)
            strcat((char *)iobuf, " (yours)");
        WPutString(iobuf, CL + 8, y + 11);
    }
    UseFont(0);
    hl = 0;
    light(0, C_THUMB);
    r = 0xFF;
    while (down())
        cw_idle = 1;
    for (;;) {
        cw_idle = 1;                    /* (the gate's player goes on) */
        i = item_at();
        if (i != 0xFF && i != hl) {
            light(hl, 0);
            light(hl = i, C_THUMB);
        }
        k = GetNextChar();
        if (!k) {
            k = getin();                /* (the ROM's codes: down $11, up $91) */
            if (k == 0x91)
                k = KEY_UP;
        }
        if (k == KEY_ESC)
            break;
        if (k == KEY_DOWN || k == KEY_UP) {
            light(hl, 0);
            hl = k == KEY_DOWN ? (hl + 1) % ntp : (hl + ntp - 1) % ntp;
            light(hl, C_THUMB);
            continue;
        }
        if (k == CR) {
            r = hl;
            break;
        }
        if (down()) {
            while (down())
                cw_idle = 1;
            r = item_at();
            break;
        }
    }
    ColorRect(CL >> 3, (CR_ + 1 - CL) >> 3, CT >> 3, ((b - CT) >> 3) + 1, 0);
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    WRecoverRectangle(CL, CT, CR_, b);
    return r;
}

/* today in Polish: "25 listopada 2026" (the months' genitive) */
static const char *const mpl[12] = {
    "stycznia", "lutego", "marca", "kwietnia", "maja", "czerwca", "lipca", "sierpnia", "wrze\xF7nia",
    "pa\xAE" "dziernika", "listopada", "grudnia"
};

static void pl_date(char *p)
{
    p = put_n(p, day);
    *p++ = ' ';
    strcpy(p, mpl[(month - 1) % 12]);
    p += strlen(p);
    *p++ = ' ';
    *p++ = '2';
    *p++ = '0';
    *p++ = (char)('0' + year / 10 % 10);
    *p++ = (char)('0' + year % 10);
    *p = 0;
}

void tb_ntempl(void)
{
    static unsigned char r, n;
    callb(bank2, f_discard);            /* (the changes saved or not first) */
    if (!t_arg)
        return;
    ntp = 1;
    strcpy(tpn[0], "Blank document");
    tpk[0] = 0;
    if (to_dir(1))
        list(1);
    if (to_dir(2))
        list(2);
    vr_cdback();
    r = choose();
    if (r == 0xFF || r >= ntp) {
        need_view = 1;
        return;
    }
    if (!tpk[r]) {
        doc_modified = 0;               /* (asked already) */
        callb(bank2, f_new);
        return;
    }
    strcpy((char *)curDirName, tdir[tpk[r] - 1]);
    strcpy(pdf_name, tpn[r]);
    strcat(pdf_name, ".cwr");
    doc_modified = 0;
    callb(bank2, f_tload);
    strcpy((char *)curDirName, vr_cd(VR_DOCS));   /* (Save as: in /DOCUMENTS) */
    if (!t_arg)
        return;
    /* the fields: {date} today's, in the template's language */
    strcpy((char *)iobuf, "{date}");
    find_set((char *)iobuf, FF_CASE);
    if (fmt_lang == LANG_PL)
        pl_date((char *)iobuf + 64);
    else {
        callb(bank14, pg_export);       /* (hf_today into iobuf) */
        strcpy((char *)iobuf + 64, (char *)iobuf);
    }
    n = (unsigned char)strlen((char *)iobuf + 64);
    replace_all(iobuf + 64, n);
    lay_fix();
    ul_reset();
    doc_modified = 0;
    doc_err = 0;
    cur = anc = 0;
    ui_moved();
    view_title();
    need_view = 1;
}

void tb_stempl(void)
{
    vr_cd(VR_DOCS);
    strcpy((char *)iobuf, "Templates");
    MkDir(iobuf);                       /* (there already: nothing) */
    if (to_dir(2))
        strcpy((char *)curDirName, tdir[1]);
    else
        vr_cdback();
    callb(bank2, f_saveas);
}


/*---------------------------------------------------------------
  (pdf.c's, overlay 15, through glue.s: pdfx.c's, then pgpanel.c's, until
  1.6's tables wanted their room) a page done: the status bar says so; Esc stops
 ---------------------------------------------------------------*/
static char *__fastcall__ put_u(char *p, unsigned int v)
{
    static unsigned int d;
    static unsigned char z;
    for (d = 10000, z = 0; d; d /= 10) {
        if (v >= d || z || d == 1) {
            *p++ = (char)('0' + v / d);
            z = 1;
        }
        v %= d;
    }
    *p = 0;
    return p;
}

unsigned char e_plat_pdf_poll(void)
{
    char *p = sl;
    strcpy(p, "Exporting page ");
    p = put_u(p + 15, pdf_page + (pdf_page < pdf_pages));
    strcpy(p, " of ");
    p = put_u(p + 4, pdf_pages);
    strcpy(p, "...");
    strcpy(sr, "Esc: stop");
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    return cw_esc();
}
