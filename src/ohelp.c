/* Veranda Write: the .docx and .odt readers' small helpers (#included by
   odocx.c and oodt.c, each an overlay of its own on the X16: a copy in
   each): names looked up by their hash (oxnames.h), numbers, lengths.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */

/* s's hash (16 bits: tools/oxnames.py's; the X16's: v/ohash.s) */
#ifdef CW_HOST
static unsigned int FASTCALL hash(const char *s)
{
    static unsigned int k;
    k = 5381;
    while (*s)
        k = (((k << 5) + k) ^ (unsigned char)*s++) & 0xFFFF;
    return k;
}
#else
#ifdef OXN_DOCX
#define hash dx_hash
#else
#define hash od_hash
#endif
unsigned int FASTCALL hash(const char *s);
#endif

/* XNAME's number in a table of oxnames.h (0: not there): its hash's
   slot, then the next ones, to an empty one */
struct ntab {
    const unsigned int *h;
    const unsigned char *l, *id;
    unsigned char m;
};
#define NTAB(x) { x##_h, x##_l, x##_i, x##_m }

static unsigned char FASTCALL nlook(const struct ntab *t)
{
    static unsigned int k;
    static const unsigned int *th;
    static const unsigned char *tl, *ti;
    static unsigned char n, i, m, id;
    k = hash(XNAME);
    n = (unsigned char)strlen(XNAME);
    th = t->h;
    tl = t->l;
    ti = t->id;
    m = t->m;
    for (i = (unsigned char)k & m; (id = ti[i]) != 0; i = (i + 1) & m)
        if (th[i] == k && tl[i] == n)
            return id;
    return 0;
}

static int FASTCALL onum(const char *s)
{
    static int v;
    static unsigned char neg;
    v = 0;
    neg = *s == '-';
    if (neg)
        ++s;
    for (; *s >= '0' && *s <= '9'; ++s)
        v = v * 10 + (*s - '0');
    return neg ? -v : v;
}

/* twips into pixels, 0 .. m */
static unsigned char FASTCALL px(int tw, unsigned char m)
{
    if (tw <= 0)
        return 0;
    tw = (tw + 9) / TW_PX;
    return tw > m ? m : (unsigned char)tw;
}

#ifdef OXN_DOCX
/* an on/off value ("1", "true", "on"; "0", "false", "off", "none") */
static unsigned char FASTCALL oon(const char *s)
{
    return !(s[0] == '0' || s[0] == 'f' || (s[0] == 'o' && s[1] == 'f') || (s[0] == 'n' && s[1] == 'o'));
}
#endif

static unsigned char FASTCALL lc(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static unsigned char FASTCALL starts(const char *s, const char *w)
{
    while (*w)
        if (lc(*s++) != *w++)
            return 0;
    return 1;
}

/* a monospaced font, by its name */
static unsigned char FASTCALL mono(const char *s)
{
    return strstr(s, "Mono") || strstr(s, "mono") || strstr(s, "Courier") || strstr(s, "Consolas")
           || strstr(s, "Code") || strstr(s, "Menlo") || strstr(s, "Monaco") || strstr(s, "Console")
           || strstr(s, "Typewriter") || strstr(s, "Fixed");
}

/* a property set (pr_t's): PR(field, value); a flag on or off */
#define SET(f, v) pr_set(PR(f, v))
#define FLAG(f, on) pr_set(PR((on) ? F_ON | (f) : F_OFF | (f), 0))
