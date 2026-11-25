/* Veranda Write on Veranda: what the spelling panel (v/spui.c, overlay
   25) and the pop-up and Tools menu (v/sppop.c, overlay 26) both have,
   compiled into each (static: an overlay cannot call another's but
   through glue). Included once by each.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
static unsigned char st = 0;            /* 0 nothing yet, 1 a word, 2 none left, 3 no dictionary */
static unsigned char wd[SP_MAXW + 1], wdl;
static char chg[SP_MAXW + 1];           /* Change to */
static char sg[NSH][SP_MAXW + 1];
static unsigned char ns, hi;            /* the suggestions, the one in Change to (0xFF: typed) */
static unsigned int nchanged;
static const char *msg;
static pos_t fws, fwe;                  /* the word found (sp_ws, sp_we are the scanner's: drawing uses them) */

/*---------------------------------------------------------------
  text: Gate Sans 12 by the renderer (Polish), the UI font by the kernel
 ---------------------------------------------------------------*/
static unsigned char gs = 0;            /* the document's plain Gate Sans 12 style is the renderer's */

static unsigned char gs_font(void)
{
    static unsigned char k;
    for (k = 0; k < nsty; ++k)
        if (sty[k].size == 12 && !(sty[k].flags & SF_KNOWN) && !strcmp(fontname[sty[k].font], def_font))
            return gs = !fl_rr(k);
    return gs = 0;
}

static unsigned int __fastcall__ gw(const char *s)
{
    static unsigned int w;
    static unsigned char c;
    for (w = 0; (c = (unsigned char)*s) != 0; ++s)
        w += CHW(c);
    return w;
}

/* a band of BAND_HF rows at y: cleared (both screens, the page's width) */
static void __fastcall__ band(unsigned int y)
{
    rr_bt = 0;
    rr_bh = BAND_HF;
    rr_clear();
    rr_y = y;
    rr_lh = BAND_HF;
}

/* s (anywhere) at x in the band */
static void __fastcall__ put(const char *s, unsigned int x)
{
    if (s != run) {
        strncpy(run, s, sizeof run - 1);
        run[sizeof run - 1] = 0;
    }
    if (gs) {
        rr_s = run;
        rr_n = (unsigned char)strlen(run);
        rr_x = x - PG_L;
        rr_top = fl_a > 10 ? 0 : 10 - fl_a;
        rr_run();
    } else
        WPutString(run, x, rr_y + 10);
}

static void blit(void)
{
    if (gs) {
        rr_ylo = rr_y;
        rr_yhi = rr_y + BAND_HF - 1;
        rr_blit();
    }
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
}

static void __fastcall__ label(const char *s, unsigned int x, unsigned int y)
{
    strncpy(run, s, sizeof run - 1);
    run[sizeof run - 1] = 0;
    WPutString(run, x, y);
}

static unsigned int __fastcall__ uw(const char *s)
{
    static unsigned int w;
    for (w = 0; *s; ++s)
        w += GetCharWidth(*s);
    return w;
}

/* the upper case of a letter of the Veranda set */
static unsigned char __fastcall__ up(unsigned char c)
{
    static const unsigned char lo[] = { 0xA2, 0xB3, 0xA8, 0xBA, 0xAE, 0xBF, 0xB9, 0xB8, 0xBD, 0xFF };
    static const unsigned char hi_[] = { 0xA1, 0xA3, 0xA6, 0xAA, 0xAC, 0xAF, 0xB2, 0xB4, 0xBC, 0xBE };
    static unsigned char k;
    if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE))
        return c - 32;
    for (k = 0; k < sizeof lo; ++k)
        if (lo[k] == c)
            return hi_[k];
    return c;
}

/* the lower case of a letter of the Veranda set */
static unsigned char __fastcall__ low(unsigned char c)
{
    static const unsigned char lo[] = { 0xA2, 0xB3, 0xA8, 0xBA, 0xAE, 0xBF, 0xB9, 0xB8, 0xBD, 0xFF };
    static const unsigned char hi_[] = { 0xA1, 0xA3, 0xA6, 0xAA, 0xAC, 0xAF, 0xB2, 0xB4, 0xBC, 0xBE };
    static unsigned char k;
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE))
        return c + 32;
    for (k = 0; k < sizeof hi_; ++k)
        if (hi_[k] == c)
            return lo[k];
    return c;
}

/* the word's case: 1 a capital first, 2 all capitals */
static unsigned char wcase(void)
{
    static unsigned char k, n;
    for (k = n = 0; k < wdl; ++k)
        if (low(wd[k]) != wd[k])
            ++n;
    if (n == wdl && wdl > 1)
        return 2;
    return low(wd[0]) != wd[0];
}

/* the suggestions (sugg.c's, in the lists' bank) in the word's case */
static void suggestions(void)
{
    static unsigned char k, c, i;
    sp_suggest();
    bk_rd(sp_xb, 0xA000u + SP_SUGG, sp_buf, 1);
    ns = sp_buf[0] > NSH ? NSH : sp_buf[0];
    c = wcase();
    for (k = 0; k < ns; ++k) {
        bk_rd(sp_xb, 0xA000u + SP_SUGG + 1 + (k << 5), iobuf, SP_MAXW + 1);   /* (main RAM; sp_buf is too small) */
        for (i = 0; iobuf[i] && i < SP_MAXW; ++i)
            sg[k][i] = (char)(c == 2 || (c == 1 && !i) ? up(iobuf[i]) : iobuf[i]);
        sg[k][i] = 0;
    }
}

/* [lo, hi) replaced by Change to (its style the first character's) */
static pos_t __fastcall__ change_at(pos_t lo, pos_t hi_)
{
    static unsigned char t, n;
    t = style_at(lo);
    lo = doc_delete(lo, hi_);
    n = (unsigned char)strlen(chg);
    memcpy(iobuf, chg, n);              /* (main RAM: overlay 6 reads it) */
    return n ? doc_insert(lo, iobuf, n, t) : lo;
}

/* the word's lower case back into sp_lw (another command may have used it) */
static void word_again(void)
{
    static unsigned char k;
    for (k = 0; k < wdl; ++k)
        sp_lw[k] = low(wd[k]);
    sp_wl = wdl;
}

