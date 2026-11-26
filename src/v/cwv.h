/* Veranda Write on Veranda: the program's own declarations (the
   document model is cw.h's).

   Where the code is (cfg/veranda.cfg, the SDK's app-c-ovl.cfg layout):
     main RAM   the model (cbuf, doc, undo, layout, format), the fonts,
                the view's drawing, the start, the dialog boxes and menus
                (their tables: the kernel reads them below $A000)
     overlay 1  keys, the pointer, the commands (mapped for good: appBank)
     overlay 2  files: New, Open, Save, Save as, Close, Quit; the format
     overlay 3  the SDK's Open and Save dialog (vrchoose.c)
     overlay 4  word wrap (layfix.c)
     overlay 5  the view's larger part (view2.c): scrolling, the bars
     overlay 6  the edits and undo (edit.c, undo.c)
     overlay 7  the clipboard (the SDK's scrap library), the window chrome,
                the scroll bar (sbar.c)
     overlay 8  the menus' making, the font cache
     overlay 9  the find panel, word count, go to, the date, the ruler
     overlay 10 geoWrite documents converted (gwimport.c)
     overlay 11 the chunked buffer's edits and the pager, with the paged
                cbufs' tables (cbufx.c, swap.c; cbuf.c's reads map it)
     overlay 12 the pager's card side: where each chunk is, the swap's
                files (swapwh.c, swapio.c); the toolbar (tbar.c)
     overlay 13 the styles and paragraphs (styles.c), the pages (pages.c),
                the toolbar's pop-ups (popup.c)
     overlay 14 the pages in the view: the rows and the bands between
                the pages, a page break's line, the header and footer
                made, Insert > Page break (pgview.c); the page setup
                (page.c, hf.c)
     overlay 15 the PDF (pdf.c, pdfx.c)
     overlay 16 the page setup panel, File > Export as PDF (pgpanel.c)
     overlay 17 a ZIP's parts inflated, their XML's tokens (ozip.c,
                oxml.c): .docx and .odt documents opened
     overlay 18 the text of a .docx or .odt put into the document, the
                styles it defines (oemit.c), what was changed (oxnote.c)
     overlay 19 the .docx reader (odocx.c)
     overlay 20 the .odt reader (oodt.c)
     overlay 21 the document as a .docx or an .odt (oexp.c, ocrc.s)
     overlay 22 the XML text of those, its templates (oparts.c)
     overlay 23 spelling: the dictionary, its cache, the hash of results,
                the word lists, the marks, live checking, Check
                spelling's search (spell.c, v/splive.c, v/spio.c)
     overlay 24 spelling's suggestions (sugg.c), its files (v/spio.c)
     overlay 25 the spelling panel (v/spui.c)
     overlay 26 the suggestions' pop-up, the Tools menu (v/sppop.c)
     overlay 27 a table's row laid out and drawn (table.c, v/tbview.c;
                record 23 of the file: 23-26 are SPELL.OVL's, v/spload.c)
     overlay 28 the table edits, the table's keys (tabedit.c, v/tbui.c;
                record 24)
     overlay 29 the Table menu, its boxes, templates; a paragraph's start
                in a row (v/tbcmd.c, tabpar.c; record 25)
     overlay 30 (test builds) the session's logs: record 26
     overlay 31 AI help (ai.c, v/aiui.c; SPELL.OVL's eleventh, W68)
   callb() runs a function of another overlay and maps the caller's back.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CWV_H
#define CWV_H

#include <veranda.h>
#include <vr.h>
#include "../cw.h"

#define MAPBANK(b) (*(volatile unsigned char *)0 = (b))

/* the screen (cards of 8 x 8; the window: vrwin.h; Veranda Look 3,
   Sunroom): the menu bar (card rows 0-1), the title band (2-3), the
   toolbar (4-5), the ruler (6-7), the text's area (8-57: the page on the
   desk, the scroll bar at its right), the status bar (58-59) */
#define TA_TOP    64                    /* the text's area: card rows 8-57 */
#define TA_BOT    ta_bot                /* (463; 399 with the find panel under it) */
#define TA_END    463
extern unsigned int ta_bot;
#define TA_ROWS   50
#define TY0       (TA_TOP + 6)          /* the first row's top (a margin above it) */
#define PG_C0     6                     /* the page: cards 6-73 */
#define PG_CW     68
#define PG_L      48
#define PG_R      591
#define TX0       64                    /* the text's left edge */
#define SB_C      78                    /* the scroll bar: cards 78-79, */
#define SB_X      624
#define SB_UP     (TA_TOP + 15)         /*   its up arrow's box to here, */
#define SB_DN     (TA_END - 15)         /*   its down arrow's from here, */
#define SB_ROWS   ((SB_DN - SB_UP - 1) >> 3)   /* the track between (card rows) */
#define RL_Y      48                    /* the ruler: card rows 6-7 */
#define TB_Y      32                    /* the toolbar: card rows 4-5, on whole cards: */
#define TB_FONT   8                     /*   the font's pop-up, */
#define TB_FONTW  104
#define TB_SIZE   120                   /*   the size's, */
#define TB_SIZEW  40
#define TB_BIU    176                   /*   the buttons (16 pixels each): B I U, */
#define TB_ALIGN  240                   /*   left, centre, right, justify, */
#define TB_LIST   320                   /*   bulleted, numbered, */
#define TB_INDENT 368                   /*   indent less, more, */
#define TB_NAME   416                   /*   the paragraph style's pop-up */
#define TB_NAMEW  120
#define C_DESK    2                     /* Look 2's CHROME: round the page */
#define C_BAR     3                     /* SIDEBAR (the mist): the scroll bar's track, the find panel */
#define C_THUMB   4                     /* SELECT: a button on, an item lit */

/* main.c */
extern unsigned char bank1, bank2, bank3, bank4, bank5, bank6, bank7, bank8, bank9, bank10, bank11, bank12, bank13, bank14, bank15, bank16;
extern unsigned char bank17, bank18, bank19, bank20, bank21, bank22, bank23, bank24, bank25, bank26, bankt;
void ox_up(void), ox_down(void);     /* (v/spload.c, overlay 16: the office overlays, W66) */
extern unsigned char bank27, bank28, bank29;   /* (1.6's tables: records 17-19) */
extern unsigned char bank31;            /* (W68: AI help, SPELL.OVL's eleventh: loaded for a command) */
/* (W68) AI help: ai_go (v/spload.c, overlay 16) loads overlay 31 and runs
   ai_cmd (v/aiui.c) with t_arg the AI menu's item, or AI_BACK (Write
   back after a request: ui.c's tick when ai_st is 2); ai_st 0 none, 1
   asked, 2 back (cw_aside), 3 still open (no answer when it was back);
   ai_act the action, ai_gen0 ai_gen when asked (main.c) */
void ai_go(void), ai_cmd(void);
#define AI_BACK 0x80
extern unsigned char ai_st, ai_act;
extern unsigned int ai_gen0;
#define TOVL 20                         /* the test build's overlay (src/v/test.c: linked as 30) */
extern unsigned char sw_tbank;
extern unsigned char cw_idle;           /* the main loop found nothing to do */
void __fastcall__ callb(unsigned char b, void (*f)(void));
#define CALL2(f) callb(bank2, f)
#define CALL4(f) callb(bank4, f)
#define CALL8(f) callb(bank8, f)
#define CALL9(f) callb(bank9, f)
void message(const char *a, const char *b);
unsigned char yesno(const char *a, const char *b);
unsigned char yesnocancel(const char *a, const char *b);
unsigned char __fastcall__ file_dlg(unsigned char mode, const char *title, char *name);
extern char msg_a[48], msg_b[64];
extern char docname[64];

/* the editing state (main.c) */
extern pos_t cur, anc;                  /* the cursor; the selection's other end */
#define HAS_SEL() (cur != anc)
#define SEL_LO() (cur < anc ? cur : anc)
#define SEL_HI() (cur < anc ? anc : cur)
extern unsigned int goal_x;             /* up and down keep it (0xFFFF: anew) */
extern unsigned char ty_style;          /* the style typing gets */
extern unsigned char ty_kept;           /* ty_style chosen (Ctrl+B ...): kept till a move */
extern unsigned char need_view;         /* the view to bring up to date */
extern unsigned char status_dirty;
extern char status_msg[40];
void set_status(const char *s);
void l1_note(void);                     /* (glue.s: latinui.c, overlay 10) letters not in the set, said */
void ui_moved(void);                    /* after the cursor moved: the typing style */
void busy_ptr(void);                    /* the busy pointer (lay_busy) */
void cw_aside(void);                    /* asideVector: put aside */
void dlg_keep(void);                    /* the dialog's bank again (after being aside) */

/* fonts.c: the font slots (fcache.c's fc_setup fills them) */
#define NFS 6
extern unsigned char *wcur, wbk[2], wlru, fs_kind[NFS], fs_size[NFS], fs_a[NFS], fs_d[NFS], fs_h[NFS];
extern unsigned char fs_bank[NFS], fs_bank2[NFS], fs_split[NFS], fs_bank3[NFS], fs_split2[NFS], fs_next, s_slot[MAXSTY], fl_s;
extern unsigned int fs_key[NFS], fs_base[NFS], fs_rowlen[NFS];
extern char fl_nm[FONT_LEN];
unsigned char FASTCALL fl_rr(unsigned char s);   /* style s for render.s (1: not drawn) */
extern unsigned char fl_slot;           /* (after fl_style) its font slot */
extern unsigned char fl_last;           /* the style fl_style set last (0xFF: none) */
void bk_wr(unsigned char b, unsigned int a, const unsigned char *src, unsigned int n);
void bk_rd(unsigned char b, unsigned int a, unsigned char *dst, unsigned int n);
#define WT_ADDR 0xBB80                  /* the slots' widths in the row bank (render.s: after ROWT) */

/* fcache.c (overlay 8): the font cache */
extern const char *fcl_name;
extern unsigned char fcl_size, fcl_bank, fcl_h, fcl_a, fcl_full, fcl_bank2, fcl_split, fcl_bank3, fcl_split2;
extern unsigned int fcl_base, fcl_rowlen;
extern unsigned char *fcl_w;
unsigned char fc_load(void);
void fc_reset(void);
void fc_setup(void);                    /* style fl_s's slot (s_slot[fl_s]) */

/* render.s (main RAM) */
extern unsigned char rr_bank, rr_n, rr_top, rr_mode, rr_fb, rr_fb2, rr_split, rr_fb3, rr_split2, rr_h, rr_a, rr_bt, rr_bh, rr_lh;
extern unsigned int rr_x, rr_fbase, rr_rowlen, rr_y, rr_ylo, rr_yhi;
extern const unsigned char *rr_w;
extern const char *rr_s;
void rr_tables(void), rr_clear(void), rr_run(void), rr_blit(void), rr_dot(void);

/* view.c (main RAM) */
#define VMAX 40
/* a row: its place, its y, its list number, its line's entry; the band
   above it (g pixels: the page before's footer, the gap, the page's
   header, W45) when it starts page pg */
/* mk (W58, v/splive.c): the spelling marks drawn on it, bit 7 a word
   on it not looked up yet */
struct vrow { pos_t pos; unsigned int y, n; struct line l; unsigned char g; unsigned int pg; unsigned char mk; };
extern struct vrow vr[VMAX];
extern unsigned char vn;                /* rows shown */
extern unsigned int top_line;
void view_frame(void);                  /* the window, the page, everything */
void view_all(void);                    /* the text drawn again */
void view_refresh(void);                /* what changed since: drawn */
void __fastcall__ view_scroll(int n);   /* n lines (down > 0) */
void view_cursor(unsigned char on);
void view_blink(void);
unsigned char cw_jiffies(void);        /* (glue.s) the ROM's jiffies, the low byte */
unsigned char cw_keys(void);           /* (glue.s) keys waiting in the ROM's queue */
void view_status(void);
void view_toolbar(void);
void view_title(void);                  /* the title band: the name, modified */
/* a frame with its corners cut (Look 3's buttons and fields) */
void __fastcall__ rbox(unsigned int l, unsigned int t, unsigned int r, unsigned int b);
extern unsigned char sb_a, sb_n;        /* the scroll bar's thumb: its card rows in the track */
/* place <-> x: on line i; lw_x the x of p on it, lw_hit the place at x */
unsigned int __fastcall__ line_x(unsigned int i, pos_t p);
pos_t __fastcall__ line_hit(unsigned int i, unsigned int x);
extern pos_t lw_start;                  /* line_x/line_hit: the line's start */
/* the row under y (0xFF: none) */
unsigned char __fastcall__ view_row(unsigned int y);
void view_fix_cursor(void);             /* the cursor's line on the screen */
void __fastcall__ v_pbreak(unsigned char i);  /* (pgview.c) row i's page break's line */
#define BAND_HF  14                     /* the bands between the pages (W45): a header's or footer's line, */
#define BAND_GAP 10                     /*   the gap */
void __fastcall__ v_band(unsigned char i);    /* (pgview.c) row i's band */
/* pages.c (overlay 13): line pg_arg's page, its line there (pg_pc,
   pg_lc), the pages (pg_tot); pg_est 1: the page a guess, 2: the pages;
   pg_idle: counting on in idle time (1: more to do); pg_first: page
   pg_arg's first line (pg_line), every page counted */
extern unsigned int pg_arg, pg_pc, pg_lc, pg_tot, pg_line;
extern unsigned char pg_est;
extern unsigned char pg_buf[];
void pg_at(void);
unsigned char pg_idle(void);
void pg_first(void);
/* pg_view: lines pg_arg .. + VMAX - 1, pv_st bit k: line pg_arg + k
   starts a page; pv_pg: line pg_arg's page */
extern unsigned char pv_st[(VMAX + 7) / 8];
extern unsigned int pv_pg;
void pg_view(void);

/* ui.c (overlay 1) */
void __fastcall__ ui_main(unsigned char opts);
void m_undo(void), m_redo(void), m_selall(void), m_cut(void), m_copy(void), m_paste(void);
void m_plain(void), m_bold(void), m_italic(void), m_uline(void), m_larger(void), m_smaller(void);
void m_about(void);
void c_new(void), c_open(void), c_save(void), c_saveas(void), c_close(void), c_quit(void);

void __fastcall__ apply_fmt(unsigned char op, unsigned char v);
extern unsigned char follow;            /* the view to go where the cursor is */
void __fastcall__ m_dyn(unsigned char bar), __fastcall__ m_item(unsigned char item);

/* files.c (overlay 2) */
void f_new(void), f_open(void), f_save(void), f_saveas(void), f_close(void), f_quit(void);
void f_startdoc(void);
void f_ldstep(void);                    /* a document being opened: a step more of it */
extern unsigned char ld_shown;          /* (opened: its first screen not drawn yet) */

/* swapio.c (overlay 12): the swap's folder, the document's file */
void sw_start(void), sw_end(void);
void __fastcall__ sw_doc(const char *name);       /* the document's file: name, in curDirName */
unsigned char __fastcall__ sw_dopen(pos_t off);   /* it opened at off (< 2: not) */
void __fastcall__ sw_dclose(unsigned char h);


/* menus.c (main RAM: the tables) */
#define MI 12                           /* a list's items at most */
#define NBAR 12                         /* the bar's: Veranda Write, File, Edit, Insert, Text, Style, Format, Font, Size, Tools, Table, AI */
struct mitem { const char *text; unsigned char type; void *p; };
struct menu { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct mitem i[MI]; };
extern struct menu mbar, msub;
extern char itext[MI][36];             /* (a group's "\x1f", the name, a tab, the keys) */
extern unsigned int barx[NBAR];         /* the bar's items (the gate's player) */
extern unsigned char menu_bar;          /* the bar's item whose list is open */
extern unsigned char fm_n;              /* the Font menu's fonts */
extern char fm_names[8][FONT_LEN];
extern unsigned char sz_n, sz_list[10];  /* the Size menu's sizes */
extern unsigned char menu_choice;       /* (a style, for sizes_fill) */
extern const unsigned char mfirst[8];

/* tools.c (overlay 8) */
extern unsigned char t_arg;             /* (main RAM) the argument of t_... */
void t_named(void);                     /* (findui.c, overlay 9) the named style t_arg: the selection's paragraphs */
void t_format(void);                    /* (overlay 9) the Format menu's item t_arg */
void t_body(void);                      /* (overlay 9) after Return at a heading's end (kind t_arg) */
void t_toolbar(void);                   /* (tbar.c, overlay 12) the toolbar and the ruler, when what they show changed */
void t_status(void);                    /* (status.c, overlay 8) the status bar (view_status) */
void t_ruler(void);                     /* (ruler.c, overlay 9) the ruler: paragraph style t_arg */
void t_sbar(void);                      /* (sbar.c, overlay 7) the scroll bar, when its thumb moved */
void menus_layout(void);                /* (tbcmd.c, overlay 29) */
void menu_make(void);                   /* msub: bar item menu_bar's list */
void sizes_fill(void);                  /* sz_list: the sizes of style menu_choice's font */
unsigned int __fastcall__ strw(const char *s);  /* s's width in the font now (a menu item's) */
/* popup.c (overlay 13): the toolbar's pop-up t_arg (4 Style, 6 Font, 7
   Size: the bar's menu) at r11, r12 wide; t_arg then the item chosen
   (0xFF: none) */
void t_popup(void);
/* findui.c (overlay 9): the Find and Replace panel (under the text,
   y FP_Y .. TA_END); its state in main RAM */
#define FP_Y      400
extern unsigned char fp_on, fp_focus, fp_flags;
extern char f_find[FD_MAX + 1], f_repl[FD_MAX + 1];
void fp_open(void);                     /* t_arg: 0 Find, 1 Replace */
void fp_key(void);                      /* keyData: t_arg 1 taken */
void fp_press(void);                    /* the pointer at r11 (x), r12 (y) */
void fp_next(void);                     /* Find next (Ctrl+G) */
void t_count(void), t_goto(void), t_date(void);   /* Word count, Go to page, Insert date and time */

/* pgpanel.c (overlay 16): the page setup and header and footer panel
   (t_arg: the item it opens at: 0 the paper, 5 the header), its keys,
   the pointer (r11, r12); File > Export as PDF (W47) */
extern unsigned char pp_on;
void pp_open(void), pp_key(void), pp_press(void);
void t_pdf(void);
/* pgview.c (overlay 14): a page break typed; the user's default page
   setup read from the card (W44) */
void t_pbreak(void);
void pg_loaddef(void);
/* (pgview.c's pg_field, for pgpanel.c: a header or footer field in Gate
   Sans 12; main RAM) */
extern const char *pf_s;
extern unsigned int pf_x, pf_y, pf_w;
extern unsigned char pf_ok;
void pg_field(void);

/* spui.c (overlay 25, W57): the spelling panel (under the text from
   y 336), the suggestions' pop-up (t_arg 1: the word at r11, r12), the
   Tools menu's item t_arg */
void spp_open(void), spp_close(void), spp_key(void), spp_press(void), sp_pop(void), t_tools(void);
void sp_fkey(void), sp_tick(void);
/* spopt.c (overlay 24, W60): Tools > Spelling options, a panel where
   the spelling panel is (spp_on 2: spui.c hands it the keys and clicks);
   so_save: sp_cfg written to WRITE.CFG (t_arg 0: done) */
void so_open(void), so_close(void), so_key(void), so_press(void), so_save(void);
void sp_bench(void);
unsigned char sp_getin(void);           /* (v/spell.s) a key from the ROM's queue, 0 none */                    /* (test builds: the gate's measurements, t_arg $7D-$7F) */   /* (sppop.c) F7, F8 (t_arg the key); the idle pass's (t_arg 1: done something) */

/* (1.6, W62-W67) tbview.c (overlay 27): a table's row drawn (draw_row's),
   its cursor (cx, cy, ch: view_cursor's), a place's x on it, the place at
   x and view_row's y (tb_hy), the selection's part of it; Up, Down, Home,
   End in it (tb_nav: t_arg 1-4 and bit 7 Shift; 1 back: done) */
void __fastcall__ tb_draw(unsigned char i);
void __fastcall__ tb_cpos(unsigned char i);
unsigned int __fastcall__ tb_lx(unsigned int i, pos_t p);
pos_t __fastcall__ tb_lh(unsigned int i, unsigned int x);
void tb_sel(unsigned char i, pos_t lo, pos_t hi);
extern unsigned int tb_hy;
void tb_nav(void);
/* tbui.c (overlay 28): a key (t_arg: keyData; t_arg 1 back: taken);
   tbcmd.c (overlay 29): the Table menu's list (itext, t_arg its items)
   and items (t_arg), Insert > Table, a page break (refused in a table),
   About, New from template, Save as template; the gate's 'L' $78 */
void tb_key(void), tb_mtext(void), tb_cmd(void), tb_tinsert(void), tb_pbreak(void), tb_about(void);
/* (tbui.c) tabedit.c's edits at the cursor for tbcmd.c: t_arg one of these
   (the Table menu's items' numbers), tr_w its numbers; t_arg back its result */
#define TD_INSERT 0
#define TD_ROWUP  1
#define TD_ROWDN  2
#define TD_COLL   3
#define TD_COLR   4
#define TD_DELROW 5
#define TD_DELCOL 6
#define TD_TABLE  7
#define TD_WIDTHS 8
#define TD_GRID   10
#define TD_ROW    11
#define TD_USED   13                    /* (oexp.c's: the cells' widths used, bits at iobuf + 160) */
#define TD_ROWAT  12                    /* (oexp.c's: the row at tb_p) */
void tb_do(void);
void tb_ntempl(void), tb_stempl(void), tb_tgate(void), tb_start(void);
void f_discard(void), f_tload(void);    /* (files.c: tbui.c's) */

/* clip.c (overlay 7) */
void clip_copy(void), clip_cut(void), clip_paste(void);

#ifdef CW_TEST
void test_start(void);
void test_tick(void);
void sp_tgate(void);                    /* (W60, v/ovlclr.c: the gate's, t_arg $7A-$7C) */
extern unsigned char test_log;
void __fastcall__ test_key(unsigned char k);
void test_drawn(void);
void test_status(void);
void test_aside(void);
void __fastcall__ play_slow(unsigned char cur);
extern unsigned char cur_on;            /* (view.c) the cursor shown */
#endif

#endif
