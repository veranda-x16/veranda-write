/* Veranda Write: the SD card's speed for the swap (DECISIONS W29):
   8 KB from a RAM bank read and written in the ways Veranda's file calls
   give, timed by the emulator's clock (x16emu's cycle counter, 8 MHz).
   Run as AUTOSTART on an SD card image (tools/swapbench/run.sh).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <veranda.h>

static char line[80];
static const char hex[] = "0123456789ABCDEF";
static unsigned long t0;
static unsigned char bk;
static char nm[40];

static unsigned long clk(void)
{
    unsigned long c;
    memcpy(&c, (void *)0x9FB8, 4);
    return c;
}

static void report(const char *what, unsigned long t)
{
    char *p = line;
    unsigned char i;
    strcpy(p, "BENCH ");
    p += 6;
    while (*what)
        *p++ = *what++;
    *p++ = ' ';
    for (i = 8; i-- > 0;)
        *p++ = hex[(t >> (i * 4)) & 15];
    *p = 0;
    LogText(line);
}

static void name(const char *pre, unsigned int n)
{
    char *p = nm;
    strcpy(p, pre);
    p += strlen(p);
    *p++ = '0' + n / 100 % 10;
    *p++ = '0' + n / 10 % 10;
    *p++ = '0' + n % 10;
    *p = 0;
}

/* 8 KB from bank bk's $A000 (main RAM code: the bank mapped for FWrite) */
static unsigned char wfile(const char *n, char mode, unsigned long at)
{
    unsigned char h, e;
    h = FOpen((void *)n, mode);
    if (h < 2)
        return 1;
    if (at != 0xFFFFFFFFul)
        FSeek(h, (unsigned int)at, (unsigned int)(at >> 16));
    *(volatile unsigned char *)0 = bk;
    e = FWrite(h, (void *)0xA000, 8192);
    FClose(h);
    return e;
}

static unsigned int rfile(const char *n, unsigned long at)
{
    unsigned char h;
    unsigned int k;
    h = FOpen((void *)n, 'r');
    if (h < 2)
        return 0;
    if (at)
        FSeek(h, (unsigned int)at, (unsigned int)(at >> 16));
    *(volatile unsigned char *)0 = bk;
    k = FRead(h, (void *)0xA000, 8192);
    FClose(h);
    return k;
}

static void go(void)
{
    unsigned int i;
    unsigned long t;
    *(volatile unsigned char *)0 = bk;
    for (i = 0; i < 8192; ++i)
        ((unsigned char *)0xA000)[i] = (unsigned char)(i * 7);
    MkDir("CWS");
    /* one file a chunk: new files */
    t0 = clk();
    for (i = 0; i < 20; ++i) {
        name("/CWS/:S", i);
        wfile(nm, 'w', 0xFFFFFFFFul);
    }
    report("new20", clk() - t0);
    /* replaced */
    t0 = clk();
    for (i = 0; i < 20; ++i) {
        name("/CWS/:S", i);
        wfile(nm, 'w', 0xFFFFFFFFul);
    }
    report("replace20", clk() - t0);
    /* read back */
    t0 = clk();
    for (i = 0; i < 20; ++i) {
        name("//CWS/:S", i);
        if (rfile(nm, 0) != 8192)
            LogText("BENCH short read");
    }
    report("read20", clk() - t0);
    /* 300 files in the folder: the last ones */
    for (i = 20; i < 300; ++i) {
        name("/CWS/:S", i);
        wfile(nm, 'w', 0xFFFFFFFFul);
    }
    t0 = clk();
    for (i = 280; i < 300; ++i) {
        name("//CWS/:S", i);
        rfile(nm, 0);
    }
    report("read20of300", clk() - t0);
    t0 = clk();
    for (i = 280; i < 300; ++i) {
        name("/CWS/:S", i);
        wfile(nm, 'w', 0xFFFFFFFFul);
    }
    report("replace20of300", clk() - t0);
    /* a big file (BIG.BIN, 4 MB, made on the host): reads at offsets */
    t0 = clk();
    for (i = 0; i < 20; ++i) {
        t = (unsigned long)(i * 197u % 500u) * 8192ul + 100;
        if (rfile("BIG.BIN", t) != 8192)
            LogText("BENCH short big read");
    }
    report("seekread20", clk() - t0);
    /* overwrite in place: 'm', seeked: byte by byte (V55) */
    t0 = clk();
    for (i = 0; i < 5; ++i) {
        t = (unsigned long)(i * 97u % 500u) * 8192ul;
        wfile("BIG.BIN", 'm', t);
    }
    report("mwrite5", clk() - t0);
    /* one sequential read of 1 MB, 8 KB at a time */
    {
        unsigned char h = FOpen("BIG.BIN", 'r');
        t0 = clk();
        *(volatile unsigned char *)0 = bk;
        for (i = 0; i < 128; ++i)
            FRead(h, (void *)0xA000, 8192);
        report("seq1MB", clk() - t0);
        FClose(h);
    }
    LogText("BENCH done");
}

int main(void)
{
    bk = BankAlloc(1);
    go();
    *(volatile unsigned char *)0x9FB4 = 0;
    asm("jmp $FFFF");
    return 0;
}
