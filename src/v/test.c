/* Veranda Write's gate: the test build's part (-DCW_TEST; overlay 7,
   its variables in main RAM; overlay 14). test_start reads the session (CW.PLY,
   tools/script.py writes it) into a RAM bank and starts the player
   (tools/play.s); the player's 'L' n makes the program log its state when
   it is next idle:

     CW STATE n len lines cur anc modified style top sum chars banks psum name

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

extern unsigned char play_bank;
void play_go(void);
void Player(void);

unsigned char test_log;
static char line[160];
static unsigned char clk[4], armed, playing;
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

void __fastcall__ test_mark(const char *what)
{
    unsigned long a, b;
    unsigned char c[4];
    char *p = line;
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

void test_aside(void)
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

/* (main RAM: it maps the script's bank for FRead). The session goes
   into overlay 14's own bank after its code ($A800 on: a bank fewer, so
   that the switcher still puts the test build aside at 512 KB) */
void test_start(void)
{
    unsigned char h;
    h = FOpen("CW.PLY", 'r');
    if (h < 2) {
        LogText("CW NO SCRIPT");
        return;
    }
    MAPBANK(bank14);
    FRead(h, (void *)0xA800, 6144);
    MAPBANK(bank1);
    FClose(h);
    play_bank = bank14;
    play_go();
    playing = 1;
    asideVector = VR_FN(cw_aside);
}

/* the state's log: overlay 8 */
#pragma code-name (push, "OVERLAY14")
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

    strncpy(p, docname, 30);
    p[30] = 0;
    LogText(line);
    sw_r0 = sw_reads;
    sw_w0 = sw_writes;
}

#pragma code-name (pop)

void test_tick(void)
{
    if (playing && !mouseSource) {
        play_go();                      /* (back from being put aside) */
        asideVector = VR_FN(cw_aside);
        LogText("CW BACK");
    }
    if (test_log)
        callb(bank14, log_state);
}

void __fastcall__ test_key(unsigned char k)
{
    if (!armed && (k == 'x' || k == 'q' || k == KEY_PGDN || k == CR)) {
        armed = k;
        clock_read(clk);
    }
}

void test_drawn(void)
{
    static char w[6] = "KEYxx";
    unsigned long a, b;
    unsigned char c[4];
    char *p = line;
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
