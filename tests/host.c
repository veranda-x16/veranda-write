/* Veranda Write's unit tests: the host's stand-ins for the X16's banks
   and fonts.

   Banks: 255 arrays of 8 KB, given in runs (host_bank_limit: the most
   there are, for the out-of-memory tests). Fonts: made-up metrics, a
   width a character from the font, the size and the character (bold one
   more), so that styles change the wrap the way real fonts do.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cw.h"
#include "../src/cbpriv.h"
#include "host.h"

unsigned char host_bank[256][8192];
unsigned char host_mapped;
unsigned int host_bank_limit = 250;
static unsigned int next_bank = 1;

void host_reset_banks(void)
{
    next_bank = 1;
}


unsigned char FASTCALL plat_bank_run(unsigned char n)
{
    unsigned int b = next_bank;
    if (b + n > host_bank_limit + 1)
        return 0;
    next_bank += n;
    memset(host_bank[b], 0xEE, 8192u * n);   /* (not cleared: as on the X16) */
    return (unsigned char)b;
}

void FASTCALL plat_bank_free(unsigned char b)
{
    (void)b;                            /* (the host's banks are not given back) */
}

unsigned char plat_free(void)
{
    unsigned int n = host_bank_limit + 1 - next_bank;
    return n > 255 ? 255 : (unsigned char)n;
}

/* the card: the swap's slots (host_swap_max of them may be written: the
   card full after that), the document's file (host_doc: what the pager
   reads a clean chunk back from) */
static unsigned char host_slot[TX_MAX + LT_MAX + 8][8192];
static unsigned char host_used[TX_MAX + LT_MAX + 8];
unsigned int host_swap_max = TX_MAX + LT_MAX + 8, host_swap_n;
const unsigned char *host_doc;
unsigned long host_doc_len;
unsigned long host_ios;
int host_wfail;                         /* (the card takes nothing: full, or out) */
int host_fails;

unsigned char FASTCALL plat_sw_write(unsigned int slot, unsigned char bank, unsigned int n)
{
    ++host_ios;
    if (host_wfail || slot >= TX_MAX + LT_MAX + 8 || n > 8192)
        return 1;
    if (!host_used[slot]) {
        if (host_swap_n >= host_swap_max)
            return 1;
        host_used[slot] = 1;
        ++host_swap_n;
    }
    memcpy(host_slot[slot], host_bank[bank], n);
    return 0;
}

unsigned char FASTCALL plat_sw_read(unsigned int slot, unsigned char bank, unsigned int n)
{
    ++host_ios;
    if (slot >= TX_MAX + LT_MAX + 8 || !host_used[slot])
        return 1;
    memcpy(host_bank[bank], host_slot[slot], n);
    return 0;
}

unsigned char FASTCALL plat_doc_read(pos_t off, unsigned char bank, unsigned int n)
{
    ++host_ios;
    if (!host_doc || off + n > host_doc_len)
        return 1;
    memcpy(host_bank[bank], host_doc + off, n);
    return 0;
}

void host_swap_reset(void)
{
    memset(host_used, 0, sizeof host_used);
    host_swap_n = 0;
}

int host_fail_ok;                       /* (the card-full test: give up, do not stop) */

unsigned char FASTCALL plat_sw_fail(unsigned char e)
{
    ++host_fails;
    if (host_fail_ok && e == SE_WRITE && host_fail_ok++ < 1000)
        return 0;
    printf("FAIL the pager: %s\n", e == SE_READ ? "a chunk cannot be read back" : "the card takes nothing");
    exit(1);
}

void host_freeze(void)
{
    host_bank_limit = next_bank - 1;
}

const unsigned char *fl_w;
unsigned char fl_bold, fl_a, fl_d;
static unsigned char widths[MAXSTY][NW];
static unsigned char known[MAXSTY];

void fl_flush(void)
{
    memset(known, 0, sizeof known);
}

void FASTCALL fl_style(unsigned char s)
{
    struct cstyle *c = &sty[s];
    unsigned char i;
    if (!known[s]) {
        for (i = 0; i < NW; ++i)
            widths[s][i] = (unsigned char)(c->size / 3 + (i * 7 + c->font * 3) % 4 + 1);
        known[s] = 1;
    }
    fl_w = widths[s];
    fl_bold = (c->flags & SF_BOLD) != 0;
    fl_a = (unsigned char)(c->size * 2 / 3 + c->font);
    fl_d = (unsigned char)(c->size - c->size * 2 / 3 + 1);
}

/* Veranda's set (its sdk/lib/vrcs.s, V135: ISO-8859-16), for format.c's
   plain text: the 40 bytes of $A0-$FF that are not Latin-1's */
static const unsigned char cs_b[40] = {
    0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA8, 0xAA, 0xAC, 0xAE,
    0xAF, 0xB2, 0xB3, 0xB4, 0xB5, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD,
    0xBE, 0xBF, 0xC3, 0xC5, 0xD0, 0xD1, 0xD5, 0xD7, 0xD8, 0xDD,
    0xDE, 0xE3, 0xE5, 0xF0, 0xF1, 0xF5, 0xF7, 0xF8, 0xFD, 0xFE
};
static const unsigned int cs_u[40] = {
    0x0104, 0x0105, 0x0141, 0x20AC, 0x201E, 0x0160, 0x0161, 0x0218, 0x0179, 0x017A,
    0x017B, 0x010C, 0x0142, 0x017D, 0x201D, 0x017E, 0x010D, 0x0219, 0x0152, 0x0153,
    0x0178, 0x017C, 0x0102, 0x0106, 0x0110, 0x0143, 0x0150, 0x015A, 0x0170, 0x0118,
    0x021A, 0x0103, 0x0107, 0x0111, 0x0144, 0x0151, 0x015B, 0x0171, 0x0119, 0x021B
};

unsigned char FASTCALL cs_byte(unsigned int u)
{
    unsigned char i;
    if (u < 0x80)
        return (unsigned char)u;
    if (u < 0xA0)
        return 0;
    for (i = 0; i < 40; ++i) {
        if (u < 0x100 && cs_b[i] == u)
            return 0;
        if (cs_u[i] == u)
            return cs_b[i];
    }
    return u < 0x100 ? (unsigned char)u : 0;
}

unsigned int FASTCALL cs_uni(unsigned char c)
{
    unsigned char i;
    for (i = 0; i < 40; ++i)
        if (cs_b[i] == c)
            return cs_u[i];
    return c;
}
