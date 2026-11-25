/* Veranda Write's gate: the test build's part (-DCW_TEST; overlay 23,
   its variables and small entry points in main RAM: the test build's
   main RAM is the program's but for them, W48). test_start reads the session (CW.PLY,
   tools/script.py writes it) into a RAM bank and starts the player
   (tools/play.s); the player's 'L' n makes the program log its state when
   it is next idle:

     CW STATE n len lines cur anc modified style top sum chars banks psum pages name

   (hex; sum: the characters and their styles, as tools/cwrd.py sums a
   document: each character c in style (font f, size z, flags g) adds
   c + 256 g + 4096 z + 65536 (the font name's bytes summed) to a 32-bit
   sum; psum: the paragraphs' styles, as tools/cwrd.py's par_sum). A key
   and its drawing are timed ("CW TIME KEYxx cycles": 'x', a
   burst's first key, PgDn, Return). Put aside (Alt+Tab), asideVector
   takes the player out of mouseSource; the main loop puts it back.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "view.h"

extern unsigned char play_bank, play_st, play_t0[4];
void play_go(void);
void Player(void);

unsigned char test_log;
static char line[160];                  /* (main RAM: LogText reads it) */
static unsigned char t_k;               /* (test_key's) */
static char mk_w[16];                   /* (test_mark's: the caller's name, copied) */

#pragma code-name ("OVERLAY23")
#pragma rodata-name ("OVL23RO")
static unsigned char clk[4], armed, playing, lat_k, lats;
static unsigned long lat_t0;
static const char hex[] = "0123456789ABCDEF";

static char *hexn(char *p, unsigned long v)
{
    unsigned char i;
    for (i = 8; i-- > 0;)
        *p++ = hex[(v >> (i * 4)) & 15];
    *p++ = ' ';
    return p;
}

static void clock_read(unsigned char *c)
{
    c[0] = *(volatile unsigned char *)0x9FB8;
    c[1] = *(volatile unsigned char *)0x9FB9;
    c[2] = *(volatile unsigned char *)0x9FBA;
    c[3] = *(volatile unsigned char *)0x9FBB;
}

/* "CW MARK what cycles": the time since the last mark (profiling) */
static unsigned char mk[4];

static void e_test_mark(void)
{
    unsigned long a, b;
    unsigned char c[4];
    char *p = line;
    const char *what = mk_w;
    clock_read(c);
    memcpy(&a, mk, 4);
    memcpy(&b, c, 4);
    memcpy(mk, c, 4);
    memcpy(p, "CW MARK ", 8);
    p += 8;
    while (*what)
        *p++ = *what++;
    *p++ = ' ';
    p = hexn(p, b - a);
    *p = 0;
    LogText(line);
}

static void e_test_aside(void)
{
    char *p = line;
    mouseSource = 0;                    /* (the player is in this program's memory) */
    TaskCtl(TASK_INFO, 0xFF, iobuf);    /* (the free banks: the switcher keeps 18) */
    memcpy(p, "CW ASIDE ", 9);
    p = hexn(p + 9, iobuf[TS_FREE]);
    p = hexn(p, (unsigned long)bk_owned << 8 | bk_free_n());
    *p = 0;
    LogText(line);
}

/* The session goes into this overlay's own bank after its code ($B000
   on: a bank fewer; 4,096 bytes, tools/script.py) */
static char ply[] = "CW.PLY";           /* (main RAM: the kernel reads it) */

static void e_test_start(void)
{
    unsigned char h;
    h = FOpen(ply, 'r');
    if (h < 2) {
        strcpy(line, "CW NO SCRIPT");   /* (the kernel reads it: main RAM) */
        LogText(line);
        return;
    }
    FRead(h, (void *)0xB000, 4096);
    FClose(h);
    play_bank = bankt;
    play_go();
    playing = 1;
    asideVector = VR_FN(cw_aside);
}

/* the state's log */
static unsigned int sw_r0, sw_w0;
static unsigned long nch, psum;

static void par_add(unsigned char k)
{
    const struct pstyle *q = &par[k];
    psum = (psum << 5) - psum + q->kind + 8 * q->align + q->left + 3 * q->right + 5 * (unsigned char)q->first
           + 7 * q->before + 11 * q->after + 13 * q->spacing;
}

static unsigned long sum_doc(void)
{
    static unsigned long sum, fs[MAXFONT];
    static pos_t p;
    static unsigned int n, i;
    static unsigned char s, c, f, k, ps;
    for (f = 0; f < nfont; ++f) {
        fs[f] = 0;
        for (k = 0; fontname[f][k]; ++k)
            fs[f] += (unsigned char)fontname[f][k];
    }
    sum = 0;
    nch = 0;
    psum = 0;
    s = 0;
    ps = 0;
    for (p = 0; p < DOC_LEN; p += i) {
        n = cb_read(&tx, p, iobuf, IOBUF);
        for (i = 0; i < n;) {
            c = iobuf[i];
            if (IS_KIND(c)) {
                if (i + 3 > n)
                    break;
                if (c == K_CS)
                    s = iobuf[i + 1];
                if (c == K_PS)
                    ps = iobuf[i + 1];
                i += 3;
                continue;
            }
            if (c == CH_CR) {
                par_add(ps);
                ps = 0;
            }
            sum += c + ((unsigned int)sty[s].flags << 8) + ((unsigned long)sty[s].size << 12) + (fs[sty[s].font] << 16);
            ++nch;
            ++i;
        }
    }
    par_add(ps);
    return sum;
}

static void log_state(void)
{
    char *p = line;
    unsigned char n = test_log;
    unsigned int r = sw_reads, w = sw_writes;
    test_log = 0;
    /* the pager's chunks read and written since the last state (before
       this one's own reading of all of it) */
    memcpy(p, "CW SWAP ", 8);
    p = hexn(p + 8, r - sw_r0);
    p = hexn(p, w - sw_w0);
    *p = 0;
    LogText(line);
    p = line;
    memcpy(p, "CW STATE ", 9);
    p += 9;
    p = hexn(p, n);
    p = hexn(p, DOC_LEN);
    p = hexn(p, lt_n);
    p = hexn(p, cur);
    p = hexn(p, anc);
    p = hexn(p, doc_modified);
    p = hexn(p, ty_style);
    p = hexn(p, top_line);
    p = hexn(p, sum_doc());
    p = hexn(p, nch);
    p = hexn(p, (unsigned long)bk_owned << 8 | bk_free_n());
    p = hexn(p, psum);
    pg_tot = 0;                         /* (W45: the pages, all of them counted) */
    if (!ld_kind) {
        pg_arg = 1;
        pg_first();
    }
    p = hexn(p, pg_tot);
    strncpy(p, docname, 30);
    p[30] = 0;
    LogText(line);
    sw_r0 = sw_reads;
    sw_w0 = sw_writes;
}

/* (W55) the partial redraw against a full one: 'L' n + $80. The text
   area's rows (TA_TOP .. TA_BOT, the page's cards) on both screens
   hashed as the edits left them, the cursor hidden; then the text
   wrapped again whole (lay_all) and the view drawn whole (v_all) and
   hashed again: "CW REDRAW n rows first last", the
   rows that differ (0: the edits drew what a full drawing draws). The
   paper's bookkeeping (clr_y, clr_pg) put back as it was, so the next
   edit is drawn as it would have been without the check */
#pragma bss-name (push, "OVL23BSS")
static unsigned char rh[TA_END - TA_TOP + 1];   /* (a row's hash, a byte: 4 KB for the session after it) */
static unsigned int rd_y, rd_n, rd_first, rd_last, rd_cy, rd_cp, rd_h;
static unsigned char rd_i, rd_s;
#pragma bss-name (pop)

static unsigned char __fastcall__ row_hash(unsigned int y)
{
    rd_h = y;
    for (rd_s = 0; rd_s < 2; ++rd_s) {
        dispBufferOn = rd_s ? ST_WR_BACK : ST_WR_FORE;
        ReadRow(iobuf, PG_C0, PG_CW, y);
        for (rd_i = 0; rd_i < PG_CW; ++rd_i)
            rd_h = (rd_h << 3 | rd_h >> 13) ^ iobuf[rd_i];
    }
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    return (unsigned char)(rd_h ^ rd_h >> 8);
}

static void redraw_check(void)
{
    char *p = line;
    cw_idle = 0;                        /* (the player waits: its frames count only when idle) */
    view_cursor(0);
    for (rd_y = TA_TOP; rd_y <= TA_BOT; ++rd_y)
        rh[rd_y - TA_TOP] = row_hash(rd_y);
    rd_cy = clr_y;
    rd_cp = clr_pg;
    lay_all();                          /* (the whole text wrapped again: a line table the edits left wrong shows too) */
    view_all();
    clr_y = rd_cy;
    clr_pg = rd_cp;
    rd_n = rd_first = rd_last = 0;
    for (rd_y = TA_TOP; rd_y <= TA_BOT; ++rd_y)
        if (rh[rd_y - TA_TOP] != row_hash(rd_y)) {
            if (!rd_n++)
                rd_first = rd_y;
            rd_last = rd_y;
        }
    view_cursor(1);
    memcpy(p, "CW REDRAW ", 10);
    p = hexn(p + 10, test_log & 0x7F);
    p = hexn(p, rd_n);
    p = hexn(p, rd_first);
    p = hexn(p, rd_last);
    *p = 0;
    test_log = 0;
    LogText(line);
}

static void e_test_tick(void)
{
    unsigned char k;
    if (playing && !mouseSource) {
        play_go();                      /* (back from being put aside) */
        asideVector = VR_FN(cw_aside);
        strcpy(line, "CW BACK");        /* (the kernel reads it: main RAM) */
        LogText(line);
    }
    k = test_log;                       /* (once: the player sets it in the interrupt) */
    if (k & 0x80)
        redraw_check();
    else if (k)
        log_state();
}

static void e_test_key(void)
{
    unsigned char k = t_k;
    ++lat_k;
    if (!armed && (k == 'x' || k == 'q' || k == KEY_PGDN || k == CR)) {
        armed = k;
        clock_read(clk);
    }
}

/* the status bar drawn after a timed key: "CW LATS cycles" since it was put */
static void e_test_status(void)
{
    unsigned long b;
    unsigned char c[4];
    if (!lats)
        return;
    lats = 0;
    clock_read(c);
    memcpy(&b, c, 4);
    memcpy(line, "CW LATS ", 8);
    hexn(line + 8, b - lat_t0)[0] = 0;
    LogText(line);
}

static void e_test_drawn(void)
{
    static char w[6] = "KEYxx";
    unsigned long a, b;
    unsigned char c[4];
    char *p = line;
    if (play_st) {
        /* a timed key ('Q', 'R') drawn: "CW LAT keys cycles", the keys
           taken since the last drawing, the time since the first was put */
        clock_read(c);
        asm("sei");
        memcpy(&a, play_t0, 4);
        play_st = 0;
        asm("cli");
        lat_t0 = a;
        lats = 1;
        memcpy(&b, c, 4);
        memcpy(p, "CW LAT ", 7);
        p = hexn(p + 7, lat_k);
        p = hexn(p, b - a);
        *p = 0;
        LogText(line);
        p = line;
    } else if (lat_k) {
        memcpy(p, "CW DRAW ", 8);       /* (keys drawn, none of them timed) */
        hexn(p + 8, lat_k)[0] = 0;
        LogText(line);
    }
    lat_k = 0;
    if (!armed)
        return;
    clock_read(c);
    memcpy(&a, clk, 4);
    memcpy(&b, c, 4);
    w[3] = hex[armed >> 4];
    w[4] = hex[armed & 15];
    armed = 0;
    memcpy(p, "CW TIME ", 8);
    p += 8;
    memcpy(p, w, 5);
    p += 5;
    *p++ = ' ';
    p = hexn(p, b - a);
    *p = 0;
    LogText(line);
}

/*---------------------------------------------------------------
  the entry points (main RAM): into this overlay
 ---------------------------------------------------------------*/
#pragma code-name ("CODE")

/* (the name is the caller's, perhaps in its overlay: copied first) */
void __fastcall__ test_mark(const char *what)
{
    unsigned char i;
    for (i = 0; i < sizeof mk_w - 1 && what[i]; ++i)
        mk_w[i] = what[i];
    mk_w[i] = 0;
    callb(bankt, e_test_mark);
}

void __fastcall__ test_key(unsigned char k)
{
    t_k = k;
    callb(bankt, e_test_key);
}

void test_aside(void) { callb(bankt, e_test_aside); }
void test_start(void) { callb(bankt, e_test_start); }
void test_tick(void) { callb(bankt, e_test_tick); }
void test_status(void) { callb(bankt, e_test_status); }
void test_drawn(void) { callb(bankt, e_test_drawn); }
