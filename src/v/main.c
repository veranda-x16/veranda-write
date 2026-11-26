/* Veranda Write on Veranda: the start, the overlays, the dialog boxes,
   the editing state (main RAM).

   The program is larger than $0801-$87FF, so it is in parts (cwv.h):
   main RAM holds the model, the fonts, the view, this file and every
   variable the kernel reads (dialog boxes, menus, strings); overlay 1
   (keys, the pointer, the commands) is mapped at $A000 for good as
   appBank, so the kernel's calls to the program find it; the other
   overlays are mapped by callb() for one command (and are appBank
   meanwhile, for their dialog boxes). Veranda Edit is built the same way
   (Veranda's DECISIONS V57); the boxes below are its kind.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrchoose.h>
#include <vrpath.h>
#include "cwv.h"
#include "../office.h"
#include "../spell.h"

/* (bank1 .. bank16, bank27 .. bank29: glue.s's, in a row; 23-26 and
   17-22 there too, v/spload.c's) */
unsigned char bankt;
unsigned char cw_idle, follow;
unsigned char ai_st, ai_act;            /* (W68, v/aiui.c) AI help: the request's state, its action */
unsigned int ai_gen0;                   /*   the text's changes when it was asked */
pos_t cur, anc;
unsigned int goal_x = 0xFFFF;
unsigned char ty_style, ty_kept, need_view, status_dirty;
char status_msg[40];
char docname[64];
/* stage 3's (W44-W47): read and written by several overlays */
struct pgset pgs;                       /* the page setup (page.c) */
unsigned int pg_th;                     /* the text's height on a page (page.c) */
unsigned char pp_on;                    /* the page setup panel is open (pgpanel.c) */
unsigned char pdf_hfw;                  /* the PDF's header or footer (pdf.c, pdfhf.c) */
unsigned int pdf_hfp, pdf_hfn;
unsigned int pdf_pages, pdf_page;       /* the PDF (pdf.c, pgpanel.c) */
unsigned long pdf_size;
const char *pdf_title;
unsigned char pdf_fh;                   /* its file (pdfx.c, pgpanel.c) */
char pdf_name[64];                      /* its name (pgpanel.c: the dialog's) */
/* stage 4's (W49-W54): the importers' (overlays 17 and 18 both read them) */
unsigned char xm_vlen;                  /* the token's value's bytes (oxml.c) */
unsigned char xm_skipa;                 /* (the reader's: this element's attributes not wanted) */
unsigned int xm_hash;                   /* (W71) the token's name's hash and length */
unsigned char xm_nlen;
struct oxcount oxc;                     /* what the conversion changed (oemit.c, v/oxnote.c) */
unsigned char pr_t, em_ps, em_rs, def_ps;   /* the readers' and the emitter's (oimp.h) */
unsigned char oz_err;                   /* (ozip.c's) */
const char *pf_s;                       /* the page setup panel's field in Gate Sans 12 (pgview.c's pg_field) */
unsigned int pf_x, pf_y, pf_w;
unsigned char pf_ok;
unsigned char oe_on, op_x;              /* the exporter's buffer's bytes; a value's (oexp.c, v/oparts.s) */
unsigned long oe_size;                  /* (oexp.c's) */
pos_t oe_rp;                            /* (1.6: oexp.c's place in the text, for v/oxnote.c's oe_poll) */

void set_status(const char *s)
{
    strncpy(status_msg, s, sizeof status_msg - 1);
    status_dirty = 1;
}

/* after the cursor moved (any overlay): the typing style, the view to
   follow it */
void ui_moved(void)
{
    ul_break();
    if (!ty_kept || !HAS_SEL()) {
        ty_kept = 0;
        ty_style = style_typing(SEL_LO());
    }
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

/* f in the overlay in bank b; the caller's bank back after */
void __fastcall__ callb(unsigned char b, void (*f)(void))
{
    unsigned char keep = *(volatile unsigned char *)0, ab = appBank;
    appBank = b;
    MAPBANK(b);
    f();
    appBank = ab;
    MAPBANK(keep);
}

/*---------------------------------------------------------------
  dialog boxes (their tables and texts in main RAM)
 ---------------------------------------------------------------*/
struct msgdlg {
    unsigned char pos;
    unsigned char t1, x1, y1, reg1;
    unsigned char t2, x2, y2, reg2;
    unsigned char b1, b1x, b1y;
    unsigned char b2, b2x, b2y;
    unsigned char b3, b3x, b3y;
    unsigned char end;
};
static const struct msgdlg msgbox = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x0E,
    OK, DBI_X_0, DBI_Y_2,
    0
};
static const struct msgdlg yesnobox = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x0E,
    YES, DBI_X_0, DBI_Y_2,
    NO, DBI_X_2, DBI_Y_2,
    0
};
static const struct msgdlg yncbox = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x0E,
    YES, DBI_X_0, DBI_Y_2,
    NO, DBI_X_1, DBI_Y_2,
    CANCEL, DBI_X_2, DBI_Y_2,
    0
};
char msg_a[48], msg_b[64];

static unsigned char box(const void *t, const char *a, const char *b)
{
    unsigned char r;
    if (a != msg_a)
        strncpy(msg_a, a, sizeof msg_a - 1);
    if (b != msg_b)
        strncpy(msg_b, b, sizeof msg_b - 1);
    r5 = (unsigned int)msg_a;
    r6 = (unsigned int)msg_b;
    view_cursor(0);
    r = DoDlgBox((void *)t);
    need_view = 1;
    status_dirty = 1;
    return r;
}

/* a box of three lines: msg_a, msg_b, msg_c (the callers fill them:
   v/oxnote.c's note of what a conversion changed) */
char msg_c[40];
static const unsigned char box3[] = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0E,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x10,
    OK, DBI_X_0, DBI_Y_2,
    0
};

void note3(void)
{
    Beep();
    r5 = (unsigned int)msg_a;
    r6 = (unsigned int)msg_b;
    r7 = (unsigned int)msg_c;
    view_cursor(0);
    DoDlgBox((void *)box3);
    need_view = 1;
    status_dirty = 1;
}

void message(const char *a, const char *b)
{
    Beep();
    box(&msgbox, a, b);
}

unsigned char yesno(const char *a, const char *b)
{
    return box(&yesnobox, a, b) == YES;
}

unsigned char yesnocancel(const char *a, const char *b)
{
    return box(&yncbox, a, b);
}

/* the Open and Save dialog (vrchoose.h), in overlay 3. It needs a RAM
   bank while it is open: one is kept for it from the start (dlg_bank),
   so that it opens even when the document has taken every other one */
static const char types[] = "Documents:CWR CVT DOCX ODT";
static unsigned char dlg_bank;

unsigned char __fastcall__ file_dlg(unsigned char mode, const char *title, char *name)
{
    static char t[32];                  /* (main RAM: the caller's overlay goes) */
    unsigned char r, b = *(volatile unsigned char *)0;
    strncpy(t, title, sizeof t - 1);
    view_cursor(0);
    vr_file_types = mode == VRF_OPEN ? types : 0;
    if (dlg_bank)
        BankFree(dlg_bank);
    MAPBANK(bank3);
    r = vr_file(mode, t, name);
    MAPBANK(b);
    dlg_bank = BankAlloc(1);
    need_view = 1;
    status_dirty = 1;
    return r;
}

/* put aside (Alt+Tab): the dialog's bank, the row bank, the font cache
   and the pool's idle runs go back to the system first (the switcher
   wants 18 banks free after it: W15), the banks taken again when the
   program is back (dlg_keep, from the main loop; the fonts read again
   when they are drawn) */
void cw_aside(void)
{
    unsigned char k;
    if (dlg_bank) {
        BankFree(dlg_bank);
        dlg_bank = 0;
    }
    BankFree(rr_bank);                  /* the row bank, the font cache (read again) */
    rr_bank = 0;
    fc_reset();
    for (k = 0; k < NFS; ++k)
        if (fs_kind[k] == 1)
            fs_kind[k] = 0;
    fl_flush();
    if (bank23)
        sp_aside();                     /* (W56: the dictionary's file closed, its cache's banks back) */
    if (ai_st)
        ai_st = 2;                      /* (W68: AI help asked: AI.RES looked at when back) */
    sw_aside();                         /* (the pool's idle runs, and chunks out to the card
                                           till Veranda has the banks the switcher needs) */
#ifdef CW_TEST
    test_aside();
#endif
}

void dlg_keep(void)
{
    sw_ulin();                          /* (the undo log's banks, if they went to the card) */
    if (!rr_bank) {
        rr_bank = BankAlloc(1);
        rr_tables();
    }
    if (!dlg_bank)
        dlg_bank = BankAlloc(1);
}

/* (W59) spelling's overlays: loaded (from SPELL.OVL, v/spload.c), else
   loaded now; 0 not there */
unsigned char sp_on(void)
{
    if (!bank23 && !sp_nofile)
        callb(bank16, sp_load);
    return bank23;
}

/* the busy pointer during a long layout (layfix.c's lay_busy: a
   function pointer called from overlay 4, so here in main RAM) */
void busy_ptr(void)
{
    SetPointer(PTR_BUSY);
}

static void idle(void)
{
    cw_idle = 1;
}

void fonts_init(void);

int main(void)
{
    unsigned char opts = r0L;
    unsigned char k;
    /* the records: overlays 1-16, then 27-29 (1.6's tables); their
       banks' variables in a row (glue.s) */
    for (k = 1; k <= 19; ++k) {
        LoadOverlay(k, 0);
        (&bank1)[k - 1] = r1L;
    }
    sw_tbank = bank11;                  /* (the paged cbufs' tables: cbuf.c maps it) */
    /* (overlays 23-26, spelling, and 17-22, the office files: not
       records; v/spload.c loads them from SPELL.OVL when they are
       wanted, W59, W66) */
#ifdef CW_TEST
    LoadOverlay(TOVL, 0);
    bankt = r1L;
#endif
    appBank = bank1;
    MAPBANK(bank1);
    vr_file_hook = idle;
    dlg_bank = BankAlloc(1);
    fonts_init();
    ui_main(opts);
    return 0;
}
