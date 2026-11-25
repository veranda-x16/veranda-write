/* Veranda Write's unit tests: spelling (W56-W59; #included by
   test_main.c: its checks and helpers). The dictionaries are the
   package's own (dict/PL.DIC, dict/EN.DIC), read through the host's
   stand-ins for the X16's file calls below, into the host's banks.

   - lookups: words with every Polish letter, in lower case and as
     written (Zażółć, ZAŻÓŁĆ: the text's case folded), English with
     apostrophes (don't, children's), hyphenated words checked as their
     parts (well-known), numbers and words with digits passed over,
     abbreviations (NATO), words longer than a dictionary word;
   - the hash: a word met again is not looked up; an unknown word kept
     against known ones;
   - the cache: a small cache (one bank, 8 slots) gives the same answers
     as a large one over every word of a text;
   - suggestions: "zolc" -> "żółć" (the Polish letters put back),
     "zazolc" -> "zażółć", "komputre" -> "komputer", English typos
     (recieve -> receive, teh -> the, dont -> don't);
   - the user's words: added (and written to their file, UTF-8, read
     back), ignored words (this session);
   - the other language: the hash emptied, the answers its dictionary's.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "../src/spell.h"

static FILE *sp_f;
static const char *sp_dir = "dict";
static unsigned char sp_def = LANG_PL, sp_ncache = 4;
unsigned long sp_seeks, sp_reads;
static char sp_upath[256] = "build/host/WRITE.UDIC";

unsigned char sp_fopen(unsigned char lang)
{
    char p[256];
    snprintf(p, sizeof p, "%s/%s", sp_dir, lang == LANG_PL ? "PL.DIC" : "EN.DIC");
    sp_f = fopen(p, "rb");
    return !sp_f;
}

void sp_fclose(void)
{
    if (sp_f)
        fclose(sp_f);
    sp_f = 0;
}

unsigned char FASTCALL sp_fseek(unsigned long off)
{
    ++sp_seeks;
    return fseek(sp_f, (long)off, SEEK_SET) != 0;
}

unsigned int sp_fread(unsigned char bank, unsigned int off, unsigned int n)
{
    ++sp_reads;
    return (unsigned int)fread(host_bank[bank] + off, 1, n, sp_f);
}

unsigned char FASTCALL sp_bank(unsigned char any)
{
    return any || bk_cheap() ? bk_get() : 0;
}

void FASTCALL sp_bfree(unsigned char b)
{
    bk_put(b);
}

unsigned char sp_nbanks(void)
{
    return sp_ncache;
}

unsigned char sp_deflang(void)
{
    if (!sp_cfg)
        return sp_def;                  /* (the settings not read: the tests' own default) */
    if ((sp_cfg & SPC_LANG) != SPC_KEYB)
        return sp_cfg & SPC_LANG;       /* (W60: the settings', as v/spio.c's) */
    return sp_def;                      /* (the keyboard's) */
}

/* the user's words: UTF-8, a word a line (v/spio.c's format) */
void sp_uload(void)
{
    FILE *f = fopen(sp_upath, "rb");
    char line[128];
    unsigned char i, n, b;
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        n = 0;
        for (i = 0; line[i] && line[i] != '\n' && line[i] != '\r'; ++i) {
            b = (unsigned char)line[i];
            if (b >= 0xC2 && b <= 0xDF && line[i + 1]) {
                b = cs_byte(((unsigned int)(b & 0x1F) << 6) | ((unsigned char)line[i + 1] & 0x3F));
                ++i;
            }
            if (b && n < SP_MAXW)
                sp_lw[n++] = sp_lc[b] ? sp_lc[b] : b;
        }
        sp_wl = n;
        if (n)
            sp_listadd(SP_USER, SP_USERMAX, &sp_ulen);
    }
    fclose(f);
}

unsigned char sp_usave(void)
{
    FILE *f = fopen(sp_upath, "wb");
    unsigned int k;
    unsigned char c;
    unsigned int u;
    if (!f)
        return 1;
    for (k = 0; k < sp_ulen; ++k) {
        c = host_bank[sp_xb][SP_USER + k];
        if (!c) {
            fputc('\n', f);
            continue;
        }
        u = cs_uni(c);
        if (u < 0x80)
            fputc((int)u, f);
        else {
            fputc(0xC0 | (u >> 6), f);
            fputc(0x80 | (u & 0x3F), f);
        }
    }
    return fclose(f) != 0;
}

/* s (UTF-8) in the set */
static unsigned char sp_set(const char *s, unsigned char *out)
{
    unsigned char n = 0, b;
    while ((b = (unsigned char)*s++) != 0) {
        if (b >= 0xC2 && b <= 0xDF && *s)
            b = cs_byte(((unsigned int)(b & 0x1F) << 6) | ((unsigned char)*s++ & 0x3F));
        out[n++] = b;
    }
    out[n] = 0;
    return n;
}

/* the word w (UTF-8, as written) looked up as the text's is: lower case */
static unsigned char sp_try(const char *w)
{
    unsigned char b[64], n = sp_set(w, b), i;
    for (i = 0; i < n; ++i) {
        sp_word[i] = b[i];
        sp_lw[i] = sp_lc[b[i]] ? sp_lc[b[i]] : b[i];
    }
    sp_wl = n;
    return sp_check();
}

/* the words of t (UTF-8) as the scanner finds them, "|" between; the
   unknown ones in capitals' place marked with "*" */
static void sp_scan(const char *t, char *out)
{
    unsigned char b[512];
    unsigned char n = sp_set(t, b), i, r;
    unsigned int u;
    unsigned char l = fmt_lang;
    ul_reset();
    doc_new();                          /* (the default language: the test's again) */
    fmt_lang = l;
    doc_insert(0, b, n, 0);
    sp_tokat(0, DOC_LEN);
    *out = 0;
    while (sp_tok()) {
        r = sp_check();
        if (r == SP_UNKNOWN && !sp_abbr())
            strcat(out, "*");
        for (i = 0; i < sp_wl; ++i) {
            char *e = out + strlen(out);
            u = cs_uni(sp_word[i]);
            if (u < 0x80) {
                e[0] = (char)u;
                e[1] = 0;
            } else {
                e[0] = (char)(0xC0 | (u >> 6));
                e[1] = (char)(0x80 | (u & 0x3F));
                e[2] = 0;
            }
        }
        strcat(out, "|");
    }
}

static unsigned char sp_has(const char *word, const char *want)
{
    unsigned char b[64], n = sp_set(word, b), k;
    char s[40], w8[64];
    unsigned char ws[64];
    memcpy(sp_lw, b, n);
    sp_wl = n;
    sp_suggest();
    sp_set(want, ws);
    for (k = 0; sp_sugg_get(k, s); ++k)
        if (!strcmp(s, (char *)ws))
            return k + 1;
    (void)w8;
    return 0;
}


/* every word of the dictionary file, decoded on the host (Writer's
   format: front-coded 1 KB blocks), sorted as the file has them */
static unsigned char (*dw)[32];
static unsigned long ndw;

static void dict_words(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char hdr[64], blk[1024], prev[32];
    unsigned int nb, du, b, i, pl, n;
    long sz;
    ndw = 0;
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fread(hdr, 1, 64, f) != 64)
        return;
    nb = hdr[6] | hdr[7] << 8;
    du = hdr[16] | hdr[17] << 8;
    dw = malloc((size_t)sz / 3 * 32);
    for (b = 0; b < nb; ++b) {
        fseek(f, (long)(du + b) * 1024, SEEK_SET);
        if (fread(blk, 1, 1024, f) != 1024)
            break;
        for (i = 0; i < 1024 && blk[i] != 0xFF; i += 2 + n) {
            pl = blk[i];
            n = blk[i + 1];
            memcpy(prev + pl, blk + i + 2, n);
            prev[pl + n] = 0;
            memcpy(dw[ndw++], prev, 32);
        }
    }
    fclose(f);
}

static int dw_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* lookups against the decoded list: dictionary words, their neighbours
   with a letter changed, added, left out (most not words), through a
   cache of one bank and of several (the suggestions' fast path: the
   block the last lookup started at) */
static void spell_against(unsigned char lang, const char *path, unsigned int n)
{
    unsigned long k, x = 12345, bad = 0, known = 0, base = 0;
    unsigned char w[40], l, r, want, op, j;
    dict_words(path);
    CHECK(ndw > 100000, "spell: %s decoded (%lu words)", path, ndw);
    fmt_lang = lang;
    for (k = 0; k < n && ndw; ++k) {
        x = x * 1103515245 + 12345;
        if (!(k & 3))
            base = (x >> 8) % ndw;      /* (four lookups near one word: the fast path) */
        strcpy((char *)w, (char *)dw[base]);
        l = (unsigned char)strlen((char *)w);
        op = (unsigned char)((x >> 3) % 4);
        j = l ? (unsigned char)((x >> 12) % l) : 0;
        if (op == 1 && l > 1)
            w[j] = (unsigned char)('a' + (x >> 20) % 26);
        else if (op == 2 && l < 30) {
            memmove(w + j + 1, w + j, l - j + 1);
            w[j] = (unsigned char)('a' + (x >> 20) % 26);
        } else if (op == 3 && l > 2)
            memmove(w + j, w + j + 1, l - j);
        l = (unsigned char)strlen((char *)w);
        want = bsearch(w, dw, ndw, 32, dw_cmp) != 0;
        memcpy(sp_lw, w, l);
        sp_wl = l;
        if (k == n / 2) {
            sp_aside();                 /* (half of them through one bank) */
            sp_ncache = 1;
        }
        r = sp_look();
        known += r == SP_KNOWN;
        if ((r == SP_KNOWN) != want && ++bad < 5)
            CHECK(0, "spell: %s looked up %u, the file has it: %u", w, r, want);
    }
    CHECK(!bad, "spell: %lu of %u lookups wrong", bad, n);
    printf("spell: %u lookups in %s against its words: %lu known, all right\n", n, path, known);
    sp_aside();
    sp_ncache = 4;
    free(dw);
}

extern unsigned long sp_finds;
/* the hash: a text of 2,000 words drawn from 900 (the dictionary's),
   checked twice: the second time with almost no lookups */
static void spell_hash_test(void)
{
    static unsigned char text[24000];
    unsigned long x = 7, f1, f2;
    unsigned int n = 0, k, i, w, l;
    fmt_lang = LANG_EN;
    dict_words("dict/EN.DIC");
    for (k = 0; k < 2000 && ndw; ++k) {
        x = x * 1103515245 + 12345;
        w = (unsigned int)(((x >> 8) % 900) * (((x >> 20) % 900)) / 900);   /* (the small ones more often) */
        w = (unsigned int)((w * 151UL) % ndw);
        l = (unsigned int)strlen((char *)dw[w]);
        for (i = 0; i < l && n < sizeof text - 2; ++i)
            text[n++] = dw[w][i];
        text[n++] = ' ';
    }
    ul_reset();
    doc_new();
    fmt_lang = LANG_EN;
    doc_insert(0, text, n, 0);
    sp_flush();
    f1 = sp_finds;
    sp_tokat(0, DOC_LEN);
    while (sp_tok())
        sp_check();
    f1 = sp_finds - f1;
    f2 = sp_finds;
    sp_tokat(0, DOC_LEN);
    while (sp_tok())
        sp_check();
    f2 = sp_finds - f2;
    printf("spell: 2,000 words checked twice: %lu lookups, then %lu\n", f1, f2);
    CHECK(f2 * 20 < f1, "spell: the second pass looked %lu words up (the first %lu)", f2, f1);
    free(dw);
}

static void test_spcfg(void);

static void test_spell(void)
{
    /* (Writer's test words: every Polish letter; "zażółć" and "gęślą", the
       pangram's, are too rare for PL.DIC's 300,000 forms) */
    static const char *const pl_known[] = {
        "komputerów", "komputerami", "są", "być", "mięso", "chcę", "łódź", "gęś", "jaźń", "książka",
        "żółw", "źle", "ćma", "pięść", "może", "więc", "się", "już", "dziękuję", "ręka", "źródło", "łąka",
        "ośmiu", "żaden", "Książka", "KSIĄŻKA", "Źródło", "ŻÓŁW", "Pięść", "ŁĄKA", "Kraków", 0
    };
    static const char *const en_known[] = {
        "don't", "children's", "receive", "colour", "color", "London", "WELL", "o'clock", 0
    };
    char out[512];
    unsigned char r, i, n;
    unsigned long reads;
    remove(sp_upath);
    fmt_lang = LANG_PL;
    CHECK(sp_ready(), "spell: PL.DIC loaded");
    for (i = 0; pl_known[i]; ++i)
        CHECK(sp_try(pl_known[i]) == SP_KNOWN, "spell: %s known", pl_known[i]);
    CHECK(sp_try("ksiazka") == SP_UNKNOWN, "spell: ksiazka unknown");
    CHECK(sp_try("KSIĄŻKĄA") == SP_UNKNOWN, "spell: KSIĄŻKĄA unknown");
    CHECK(sp_try("komputre") == SP_UNKNOWN, "spell: komputre unknown");
    /* the hash: no lookup for a word met again */
    reads = sp_reads;
    sp_try("komputerami");
    sp_try("komputre");
    CHECK(sp_reads == reads, "spell: words met again read nothing (%lu reads)", sp_reads - reads);
    /* the scanner: words, numbers, hyphens, codes */
    sp_scan("Książka, ŻÓŁW: źródło 1984 r. x2 biało-czerwony ksiazka NATO PKP ABCDEFG gęś.", out);
    CHECK(!strcmp(out, "Książka|ŻÓŁW|źródło|biało|czerwony|*ksiazka|NATO|PKP|*ABCDEFG|gęś|"),
          "spell: the words %s", out);
    /* suggestions */
    CHECK(sp_has("zolc", "żółć"), "spell: zolc -> żółć");
    CHECK(sp_has("ksiazka", "książka"), "spell: ksiazka -> książka");
    CHECK(sp_has("jazn", "jaźń"), "spell: jazn -> jaźń");
    CHECK(sp_has("lodz", "łódź"), "spell: lodz -> łódź");
    CHECK(sp_has("moze", "może"), "spell: moze -> może");
    CHECK(sp_has("komputre", "komputer"), "spell: komputre -> komputer");
    CHECK(sp_has("piesc", "pięść"), "spell: piesc -> pięść");
    CHECK(sp_has("prosze", "proszę"), "spell: prosze -> proszę");
    CHECK(sp_has("żeka", "rzeka"), "spell: żeka -> rzeka (rz/ż)");
    /* the user's words and the ignored ones */
    CHECK(sp_try("Spoczynskiego") == SP_UNKNOWN, "spell: a name unknown");
    sp_set("spoczynskiego", sp_lw);
    sp_wl = (unsigned char)strlen((char *)sp_lw);
    CHECK(sp_adduser() == 0, "spell: added to the user's words");
    CHECK(sp_try("Spoczynskiego") == SP_KNOWN, "spell: the user's word known");
    sp_set("źdźbłoxyz", sp_lw);
    sp_wl = (unsigned char)strlen((char *)sp_lw);
    CHECK(sp_adduser() == 0, "spell: a Polish user word added");
    CHECK(sp_try("qwertzuiop") == SP_UNKNOWN, "spell: qwertzuiop unknown");
    sp_set("qwertzuiop", sp_lw);
    sp_wl = 10;
    sp_ignore();
    CHECK(sp_try("qwertzuiop") == SP_KNOWN, "spell: ignored");
    sp_flush();
    CHECK(sp_try("qwertzuiop") == SP_KNOWN, "spell: ignored, the hash emptied");
    CHECK(sp_try("źdźbłoxyz") == SP_KNOWN, "spell: the Polish user word, the hash emptied");
    {
        FILE *f = fopen(sp_upath, "rb");
        char t[128];
        size_t k = f ? fread(t, 1, sizeof t - 1, f) : 0;
        t[k] = 0;
        if (f)
            fclose(f);
        CHECK(!strcmp(t, "spoczynskiego\nźdźbłoxyz\n"), "spell: the user's file %s", t);
    }
    /* English */
    fmt_lang = LANG_EN;
    CHECK(sp_ready(), "spell: EN.DIC loaded");
    for (i = 0; en_known[i]; ++i)
        CHECK(sp_try(en_known[i]) == SP_KNOWN, "spell: %s known", en_known[i]);
    CHECK(sp_try("recieve") == SP_UNKNOWN, "spell: recieve unknown");
    CHECK(sp_try("książka") == SP_UNKNOWN, "spell: Polish unknown in English");
    CHECK(sp_try("Spoczynskiego") == SP_KNOWN, "spell: the user's word in English too");
    sp_scan("Don't say it's well-known: the children's 3rd book, 2024, teh recieve 'quoted' dogs' X", out);
    CHECK(!strcmp(out, "Don't|say|it's|well|known|the|children's|book|*teh|*recieve|quoted|dogs|"),
          "spell: the English words %s", out);
    CHECK(sp_has("recieve", "receive") == 1, "spell: recieve -> receive first");
    CHECK(sp_has("teh", "the"), "spell: teh -> the");
    CHECK(sp_has("dont", "don't"), "spell: dont -> don't");
    CHECK(sp_has("acommodate", "accommodate"), "spell: acommodate -> accommodate");
    CHECK(sp_has("seperate", "separate"), "spell: seperate -> separate");
    /* a small cache answers as a large one: every word of a text, both
       caches, both languages */
    {
        static const char *const words[] = { "the", "zebra", "aardvark", "quick", "brown", "fox", "jumps",
            "over", "lazy", "dog", "recieve", "zymurgy", "abacus", "middle", "xylophone", "yellow",
            "nonexistentword", "receive", "accommodation", "a", "zz", 0 };
        unsigned char res[32];
        for (n = 0; n < 2; ++n) {
            sp_aside();
            sp_ncache = n ? 1 : 8;
            sp_flush();
            CHECK(sp_ready(), "spell: the cache taken again (%u banks)", sp_ncache);
            for (i = 0; words[i]; ++i) {
                sp_set(words[i], sp_lw);
                sp_wl = (unsigned char)strlen((char *)sp_lw);
                r = sp_look();
                if (!n)
                    res[i] = r;
                else
                    CHECK(r == res[i], "spell: %s the same with a small cache", words[i]);
            }
        }
        sp_ncache = 4;
        sp_aside();
        CHECK(sp_ready(), "spell: the cache again");
    }
    /* back to Polish: the hash emptied */
    fmt_lang = LANG_PL;
    CHECK(sp_try("książka") == SP_KNOWN, "spell: Polish again");
    CHECK(sp_try("receive") == SP_UNKNOWN, "spell: English unknown in Polish");
    sp_end();
    printf("spell: %lu reads, %lu seeks\n", sp_reads, sp_seeks);
    remove(sp_upath);
    spell_hash_test();
    spell_against(LANG_PL, "dict/PL.DIC", 20000);
    spell_against(LANG_EN, "dict/EN.DIC", 20000);
    test_spcfg();
}

/* (W60) the settings' file, WRITE.CFG (src/spcfg.c), written and read
   back through a file as v/spload.c does: missing, short, of another
   kind, version 0, a later version's longer file, a language or flags
   it does not know; every setting round trip; the default language
   from it */
static unsigned char cfg_file(const char *path, const unsigned char *b, unsigned int n)
{
    unsigned char r[16];
    FILE *f;
    if (b) {
        f = fopen(path, "wb");
        fwrite(b, 1, n, f);
        fclose(f);
    }
    f = fopen(path, "rb");
    n = 0;
    if (f) {                            /* (v/spload.c reads up to 16 bytes) */
        n = (unsigned int)fread(r, 1, 16, f);
        fclose(f);
    }
    remove(path);
    return sp_cfgparse(r, n);
}

static void test_spcfg(void)
{
    static const char path[] = "build/host/WRITE.CFG";
    unsigned char b[16], c, l, k, f;
    remove(path);
    CHECK(cfg_file(path, 0, 0) == LANG_EN, "spcfg: no file: the defaults (English)");
    sp_cfgmake(b, SPC_NOMARK | SPC_ASK | LANG_PL);
    CHECK(!memcmp(b, "WCFG\x01\x03\x02", SPC_LEN), "spcfg: the bytes");
    for (k = 0; k < SPC_LEN; ++k)
        CHECK(cfg_file(path, b, k) == LANG_EN, "spcfg: %u bytes (short): the defaults", k);
    CHECK(cfg_file(path, b, SPC_LEN) == (SPC_NOMARK | SPC_ASK | LANG_PL), "spcfg: read back");
    memcpy(b, "CWPG", 4);
    CHECK(cfg_file(path, b, SPC_LEN) == LANG_EN, "spcfg: another kind of file: the defaults");
    memcpy(b, "WCFG\x00\x03\x02", SPC_LEN);
    CHECK(cfg_file(path, b, SPC_LEN) == LANG_EN, "spcfg: version 0: the defaults");
    memcpy(b, "WCFG\x02\x01\x02\x55\x66\x77", 10);
    CHECK(cfg_file(path, b, 10) == (SPC_NOMARK | LANG_PL), "spcfg: a later version's longer file: its first fields");
    memcpy(b, "WCFG\x01\xFE\x07", SPC_LEN);
    CHECK(cfg_file(path, b, SPC_LEN) == (SPC_ASK | LANG_EN), "spcfg: an unknown language: English; unknown flags passed over");
    memcpy(b, "WCFG\x01\x00\x00", SPC_LEN);
    CHECK(cfg_file(path, b, SPC_LEN) == LANG_EN, "spcfg: language 0: English");
    memcpy(b, "WCFG\x01\x00\x03", SPC_LEN);
    CHECK(cfg_file(path, b, SPC_LEN) == SPC_KEYB, "spcfg: the keyboard's");
    for (f = 0; f < 4; ++f)
        for (l = LANG_EN; l <= SPC_KEYB; ++l) {
            c = (unsigned char)(f << 2 | l);
            sp_cfgmake(b, c);
            CHECK(cfg_file(path, b, SPC_LEN) == c, "spcfg: %02X round trip", c);
            b[7] = 0x5A;
            CHECK(cfg_file(path, b, 8) == c, "spcfg: %02X with a byte more", c);
        }
    /* the default language: English unless the settings say otherwise,
       whatever the keyboard; the keyboard's only when chosen */
    l = sp_def;
    for (k = 0; k < 2; ++k) {
        sp_def = k ? LANG_PL : LANG_EN; /* (Veranda's keyboard setting: Polish or not) */
        sp_cfg = sp_cfgparse(b, 0);     /* (no settings file) */
        CHECK(sp_deflang() == LANG_EN, "spcfg: no settings, the keyboard %s: English", k ? "Polish" : "not");
        sp_cfg = LANG_PL | SPC_NOMARK;
        CHECK(sp_deflang() == LANG_PL, "spcfg: Polish from the settings");
        sp_cfg = LANG_EN;
        CHECK(sp_deflang() == LANG_EN, "spcfg: English from the settings");
        sp_cfg = SPC_KEYB;
        CHECK(sp_deflang() == sp_def, "spcfg: the keyboard's from the settings");
    }
    sp_cfg = 0;
    sp_def = l;
}

/* (tools/script.py's) the suggestions for CW_SUGG's words (UTF-8, lower
   case), CW_LANG "en" or "pl": a line each, the word then its
   suggestions, UTF-8, tabs between */
static void sugg_list(void)
{
    char buf[512], *w, out[40];
    unsigned char b[64], n, k, i;
    unsigned int u;
    fmt_lang = getenv("CW_LANG") && !strcmp(getenv("CW_LANG"), "en") ? LANG_EN : LANG_PL;
    strncpy(buf, getenv("CW_SUGG"), sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (w = strtok(buf, " "); w; w = strtok(0, " ")) {
        n = sp_set(w, b);
        memcpy(sp_lw, b, n);
        sp_wl = n;
        printf("%s", w);
        printf("\t%s", sp_check() == SP_KNOWN ? "known" : "unknown");
        memcpy(sp_lw, b, n);
        sp_wl = n;
        sp_suggest();
        for (k = 0; sp_sugg_get(k, out); ++k) {
            putchar('\t');
            for (i = 0; out[i]; ++i) {
                u = cs_uni((unsigned char)out[i]);
                if (u < 0x80)
                    putchar((int)u);
                else {
                    putchar(0xC0 | (u >> 6));
                    putchar(0x80 | (u & 0x3F));
                }
            }
        }
        putchar('\n');
    }
}
