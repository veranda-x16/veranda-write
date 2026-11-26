/* Veranda Write: spelling (DECISIONS W56-W59; docs/SPELLING.md).

   Ported from Writer, the user's own earlier X16 word processor
   (~/Projects/Writer, src/spell: dict.c, dict_list.c, dict_user.c,
   spell.c, suggest.c): its dictionary format (<LANG>.DIC: words
   front-coded in 1 KB blocks, a two-level index of the blocks' first
   words), its lookup (the top index in RAM, a key page, a block scanned
   by fast_scan), its word lists (user words, ignored words), its
   suggestions (diacritics restored first: "zolc" -> "żółć", the
   Polish letters put back), then the language's confusions, transpositions,
   deletions, substitutions, insertions). Changed for Write: the cache is
   least recently used (Writer's went round), as many banks as the
   machine can spare; every result is kept in a hash of known and unknown
   words (a word met again costs no lookup); the file stays open for a
   whole operation and a block that follows the last one read is read
   without a seek; the words are Write's text (codes inside a word passed
   over), the Veranda set (ISO-8859-16) whatever the language.

   Where (the X16): the lookups, the hash, the lists, the marks drawn
   under unknown words, the live checking and Check spelling's search in
   overlay 23 (spell.c, v/splive.c, v/spio.c); the suggestions in overlay
   24 (sugg.c); the panel, the pop-up, the Tools menu's commands in
   overlay 25 (v/spui.c); the block scan, the key comparison and the
   reads into a bank in main RAM (v/spell.s: they map the cache's banks
   at $A000, where the overlays are).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
   Writer's code taken over: the user's own (Writer, MIT). */
#ifndef CW_SPELL_H
#define CW_SPELL_H

#include "cw.h"

#define SP_MAXW   31                    /* the longest word looked up (bytes) */
#define SP_BLOCK  1024                  /* a dictionary unit: a block or a key page */
#define SP_KEY    16
#define SP_PAGEKEYS 64
#define SP_HDR    64
#define SP_TOPMAX 128                   /* key pages in the top index (2 KB: 8,192 blocks) */
#define SP_UNITS  2048                  /* units of a dictionary file the cache maps (2 MB) */
#define SP_CBMAX  24                    /* cache banks at most (192 slots: 192 KB of the dictionary) */

/* the hash bank: the results (512 sets of 4 entries of 3 bytes: a
   16-bit tag, the length and the result) then the units' slots */
#define SP_HSETS  512
#define SP_WAYS   4
#define SP_UMAP   6144                  /* unit -> slot + 1 (0: not in the cache) */
/* the lists bank: the top index, the user's words, the ignored words,
   the suggestions */
#define SP_TOP    0
#define SP_USER   2048
#define SP_USERMAX 3072
#define SP_IGN    5120
#define SP_IGNMAX 2048
#define SP_SUGG   7168                  /* count, then SP_NSUGG words of 32 bytes */
#define SP_NSUGG  8

/* results */
#define SP_UNKNOWN 0
#define SP_KNOWN   1
#define SP_UNSURE  2                    /* the dictionary could not be read */
#define SP_NONE    0xFF                 /* (sp_probe: not in the hash) */

/* languages: fmt_lang (format.c: CWRD 1.5, INFO's last byte) */
#define LANG_NONE 0                     /* (a document of 1.0-1.4: the default) */
#define LANG_EN   1
#define LANG_PL   2

/* errors (sp_err) */
#define SPE_OK    0
#define SPE_NODIC 1                     /* the dictionary is not on the card */
#define SPE_MEM   2                     /* no RAM bank for it */
#define SPE_READ  3

/* main RAM (v/spell.s; the host's in spell.c): what the overlays share */
extern unsigned char sp_lw[SP_MAXW + 1]; /* the word looked up: lower case */
extern unsigned char sp_wl;
extern unsigned char sp_word[SP_MAXW + 1];  /* (as written: Check spelling's, the pop-up's) */
extern unsigned char sp_live;           /* marking as you type: on */
extern unsigned char sp_pend;           /* rows on the screen to check (live) */
extern unsigned char sp_err;
extern unsigned char sp_kj;             /* cw_jiffies at the last key */
extern unsigned char sp_buf[24];        /* (bk_rd's and bk_wr's: main RAM) */
/* (the gate's measurements) 1: no hash of results, 2: a cache of two
   banks (Writer's 16 KB): spelling as Writer did it, taken over */
extern unsigned char sp_tflags;
extern unsigned char sp_ncb, sp_busy;   /* the cache's banks; a lookup using them (main RAM: fonts.c's plat_bank_run gives them back when memory is short) */
extern pos_t sp_ws, sp_we;              /* the word found: its place */
extern unsigned char fmt_lang;          /* (format.c) the document's language */

/* the letters (spell.c): sp_lc[c] the lower case of letter c, 0 not a letter */
extern const unsigned char sp_lc[256];
extern const unsigned char sp_uc[256];  /* the upper case of a letter (c itself: none) */

/* overlay 23 (spell.c) */
unsigned char sp_ready(void);           /* banks, the dictionary of fmt_lang (its default first): 1 ready */
unsigned char sp_check(void);           /* sp_lw: SP_KNOWN, SP_UNKNOWN, SP_UNSURE (kept in the hash) */
unsigned char sp_look(void);            /* the same, the hash not written (suggestions) */
unsigned char sp_probe(void);           /* the hash only: its result, SP_NONE */
void sp_ignore(void);                   /* sp_lw ignored from now (this session) */
unsigned char sp_adduser(void);         /* sp_lw into the user's words (and their file): 0 done */
void sp_flush(void);                    /* the hash emptied (the lists changed, another language) */
void FASTCALL sp_drop(unsigned char all);   /* 0: sp_flush; 1: every bank given back, all forgotten */
/* (v/spload.c, overlay 16) the spelling overlays read from SPELL.OVL
   (t_arg 1: done), given back (with sp_drop(1)); sp_on (main RAM):
   loaded, else loaded now; 0 not there */
void sp_load(void), sp_unload(void);
unsigned char sp_on(void);
extern unsigned char sp_nofile;         /* (main RAM) SPELL.OVL not there or not this build's */
/* (W60) the settings, /VERANDA/.SYSTEM/WRITE.CFG (src/spcfg.c: the
   format; v/spcfgio.c, overlay 7: read at the start, written when the
   Tools menu changes them); sp_cfg (main RAM): LANG_EN alone the defaults */
#define SPC_LANG   0x03                 /* the language of new documents: LANG_EN (the default), LANG_PL, SPC_KEYB */
#define SPC_KEYB   3                    /* (SPC_LANG) the keyboard's: Polish when Veranda's keyboard setting is */
#define SPC_NOMARK 0x04                 /* misspellings not marked as you type */
#define SPC_ASK    0x08                 /* spelling only when asked: nothing loaded or marked before */
#define SPC_NOAI   0x10                 /* (W68) AI help off: no AI menu (the file's flags' bit 2) */
#define SPC_LEN    7                    /* the file's bytes (version 1) */
extern unsigned char sp_cfg;
unsigned char FASTCALL sp_cfgparse(const unsigned char *b, unsigned int n);
void FASTCALL sp_cfgmake(unsigned char *b, unsigned char c);
void sp_cfgload(void);                  /* sp_cfg from the file, sp_live from it */
void sp_cfgsave(void);                  /* sp_cfg to the file, sp_live from it (t_arg 0: written) */
void ovl_clear(void);                   /* (v/ovlclr.c) every record overlay's variables 0 (the start) */
void sp_end(void);                      /* the dictionary's file closed (an operation's end) */
void sp_aside(void);                    /* put aside: the file closed, the cache's banks given back */
unsigned char sp_deflang(void);         /* the default language (W60: the settings', else the keyboard's: Polish or not) */
unsigned char sp_nodic(void);           /* the dictionary of the language is not on the card */
/* the words of the text from..to: sp_tok fills sp_word (as written), sp_lw,
   sp_wl (lower case), sp_ws, sp_we (its place) with the next word to
   check; 0 at the end. sp_tokpos: where the scan is */
void FASTCALL sp_tokat(pos_t from, pos_t to);
unsigned char sp_tok(void);
pos_t sp_tokpos(void);
unsigned char sp_abbr(void);            /* the word is an abbreviation: all capitals, 5 letters at most */
/* the case: 0 lower (or mixed), 1 first capital, 2 all capitals */
unsigned char FASTCALL sp_case(const unsigned char *w, unsigned char n);

/* the platform (v/spio.c; the host's in tests/test_spell.c) */
unsigned char sp_fopen(unsigned char lang);   /* the language's .DIC opened: 0 done */
void sp_fclose(void);
unsigned char FASTCALL sp_fseek(unsigned long off);   /* 0 done */
unsigned int sp_fread(unsigned char bank, unsigned int off, unsigned int n);  /* into bank at off: bytes */
void sp_uload(void);                    /* the user's words read (sp_ulist) */
unsigned char sp_usave(void);           /* written: 0 done */
unsigned char FASTCALL sp_bank(unsigned char any); /* a bank of the document's pool (0: none; any 0:
                                           only if no chunk goes out to the card for it) */
void FASTCALL sp_bfree(unsigned char b);   /* back to the pool (0: nothing) */
unsigned char sp_nbanks(void);          /* cache banks to take now */
/* the user's words, NUL after each (spell.c): add one (0 done) */
unsigned char FASTCALL sp_listadd(unsigned int at, unsigned int max, unsigned int *used);
extern unsigned int sp_ulen, sp_ilen;
extern unsigned char sp_xb;             /* the lists' bank */

/* the bank's bytes (main RAM on the X16: fonts.c's bk_rd and bk_wr; the
   host's: tests/test_spell.c); off from the bank's start */
#ifdef CW_HOST
void FASTCALL sp_rd(unsigned char b, unsigned int off, unsigned char *dst, unsigned int n);
void FASTCALL sp_wr(unsigned char b, unsigned int off, const unsigned char *src, unsigned int n);
#else
void bk_rd(unsigned char b, unsigned int a, unsigned char *dst, unsigned int n);
void bk_wr(unsigned char b, unsigned int a, const unsigned char *src, unsigned int n);
#define sp_rd(b, off, dst, n) bk_rd(b, 0xA000u + (off), dst, n)
#define sp_wr(b, off, src, n) bk_wr(b, 0xA000u + (off), src, n)
#endif
/* (v/spell.s on the X16) the block at bank b, off scanned for sp_lw: 1
   found, 0 not there (it sorts inside), 2 it sorts after the block;
   the key at bank b, off against sp_lw cut to a key: -1 key < word, 0
   equal, 1 key > word */
#ifdef CW_HOST
unsigned char FASTCALL sp_bscan(unsigned char b, unsigned int off);
signed char FASTCALL sp_kcmp(unsigned char b, unsigned int off);
#else
extern unsigned char sp_bk;             /* (their bank) */
unsigned char __fastcall__ sp_bscan1(unsigned int a);
signed char __fastcall__ sp_kcmp1(unsigned int a);
#define sp_bscan(b, off) (sp_bk = (b), sp_bscan1(0xA000u + (off)))
#define sp_kcmp(b, off) (sp_bk = (b), sp_kcmp1(0xA000u + (off)))
#endif
/* Check spelling (v/splive.c): the next unknown word from sp_from (to
   the end, then from the start to sp_stop: sp_wrap) into sp_word, sp_lw,
   sp_ws, sp_we: 1; 0 none; 2 stopped (Esc); 3 no dictionary */
extern pos_t sp_from, sp_stop;
extern unsigned char sp_wrap, spp_on;
unsigned char sp_next(void);
/* marks (v/splive.c): row i's unknown words underlined (draw_row) */
#define MK_O 16                         /* (marks' starts and ends a row: 8 words) */
extern unsigned char mk_n, mk_k;        /* (view.c, main RAM: draw_row's) */
extern unsigned int mk_nx, mk_o[MK_O], mk_x[MK_O];
void FASTCALL mk_pre(unsigned char i);  /* row i's unknown words: mk_o, mk_n; vr[i].mk */
void FASTCALL mk_dots(unsigned char i); /* their dotted lines (mk_x, as draw_row found them) */
unsigned char sp_idle(void);            /* live checking: a step (1: it did something) */

/* overlay 24 (sugg.c): suggestions for sp_lw into the lists bank at
   SP_SUGG (count, then the words, NUL-terminated, 32 bytes each) */
void sp_suggest(void);
#ifdef CW_HOST
unsigned char sp_sugg_get(unsigned char i, char *out);
#endif

#endif
