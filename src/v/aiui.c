/* Veranda Write on Veranda: AI help (DECISIONS W68), the AI menu's
   commands and the hand-off to Veranda Ask (Veranda's docs/AIREQ.md, its
   V198 and V199). Overlay 31, not a record: SPELL.OVL's eleventh, read
   into a bank for a command and given back after (v/spload.c's ai_go).

   A command (Rewrite ... Ask about this text): the selection, or with
   none the paragraph at the cursor (selected, so the user sees what goes),
   checked by ai.c (one cell of a table at most, 8,192 bytes); for Ask a
   question (a box, 60 characters); then in /VERANDA/.SYSTEM: AI.OUT and
   AI.RES of an earlier request deleted, AI.IN (the text), AI.REQ
   ("AIREQ 1", the action, the question, "back Veranda Write"), START.REQ
   (Veranda Ask's path), and Write put aside (TaskCtl TASK_ASIDE): the
   desktop starts Veranda Ask, which shows what would go and sends it
   only when the user clicks Send. ai_st 1: asked.

   Back (cw_aside made ai_st 2; ui.c's tick calls ai_go with AI_BACK), or
   AI > Check for the answer: AI.RES read. None: Veranda Ask has not
   answered (the user came back with Alt+Tab): said, ai_st 3, looked at
   again the next time Write comes back or is asked. "ok": AI.OUT put in
   (ai.c: in place of the range, or after its paragraph for Summarise and
   Ask), one undo step, the answer selected; unless the text changed
   since it was asked (ai_gen), then said and not used. "cancelled": the
   status line. "error not-set-up": how to set Veranda Ask up; other
   errors: their message. AI.RES and AI.OUT deleted after.

   (W70) A request while an earlier one is open (ai_st) and a Veranda Ask
   is aside: refused (that one holds the earlier request on its screen;
   its Send would answer that), the earlier range kept. Back with no
   AI.RES but AI.REQ still there: Veranda Ask did not start; said, the
   request's files deleted.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#pragma code-name ("OVERLAY31")
#pragma rodata-name ("OVL31RO")
#pragma bss-name ("OVL31BSS")
#pragma data-name ("OVL31DATA")

/* iobuf (main RAM: the kernel reads it) while a command runs: a box's
   table, its two lines, the question, a file's name */
#define DLG  ((unsigned char *)iobuf)
#define LN1  ((char *)iobuf + 64)
#define LN2  ((char *)iobuf + 160)
#define QB   ((char *)iobuf + 256)       /* (the box's field only: copied to aq as the box closes) */
#define QK   (iobuf + 320)              /* (W72) the field's key stub (qstub: code, main RAM) */
#define PATH ((char *)iobuf + IOBUF - 64)
#define QMAX 60                         /* the question's characters (the box's field; aq) */
#define PIECE 192                       /* AI.OUT read a piece at a time, into iobuf + AI_RUN */

static const char *const act[] = { "rewrite", "shorten", "grammar", "translate-en", "translate-pl", "summarise", "ask" };
static const char *const done[] = {
    "Rewritten by Veranda Ask", "Shortened by Veranda Ask", "Grammar and spelling fixed",
    "Translated into English", "Translated into Polish", "Summary added after the paragraph",
    "Answer added after the paragraph"
};
#define A_ASK   6
#define A_CHECK 7
#define A_ABOUT 8

static char tl[72];                     /* (a line: the gate's log's, an error's message) */
#ifdef CW_TEST
/* the gate's log ("CW AI ...": LogText reads main RAM, so in LN1's room) */
static void FASTCALL log(const char *s)
{
    strcpy(LN1, "CW AI ");
    strcat(LN1, s);
    LogText(LN1);
}
#else
#define log(s)
#endif

/*---------------------------------------------------------------
  the boxes: wider than the default (448 pixels: two lines of about 60
  characters), their table and texts in iobuf. (W69) Their position's Y
  fields are words (WIDE_DB): as bytes the bottom (271) was 15, and the
  kernel drew no frame, no paper and no shadow, only the top line: the
  box's text over the page's
 ---------------------------------------------------------------*/
#define BX_T 176
#define BX_L 96
static const unsigned char wbox[] = {
    SET_DB_POS | WIDE_DB | 1,           /* (W69: word Y fields: its bottom, 271, is past a byte's 255) */
    BX_T, 0, (BX_T + 95) & 255, (BX_T + 95) >> 8, BX_L, 0, (BX_L + 447) & 255, (BX_L + 447) >> 8,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y + 8, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y + 8, 0x0E,
    OK, DBI_X_0, DBI_Y_2,
    0
};
static const unsigned char qbox[] = {
    SET_DB_POS | WIDE_DB | 1,           /* (W69: word Y fields: its bottom, 271, is past a byte's 255) */
    BX_T, 0, (BX_T + 95) & 255, (BX_T + 95) >> 8, BX_L, 0, (BX_L + 447) & 255, (BX_L + 447) >> 8,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0E,
    DBGETSTRING, TXT_LN_X, TXT_LN_3_Y + 4, 0x10, QMAX,
    DB_USR_ROUT, 0, 0,                  /* (W72: QK + 24, set by dlg: Polish letters in the field) */
    OK, DBI_X_0, DBI_Y_2,
    CANCEL, 49, DBI_Y_2,
    0
};

/* (W72) the question, in a buffer of its own from the box's closing to
   AI.REQ's writing: in QB (iobuf) it was the text's tail by then, AI.IN
   written a 512-byte piece at a time through iobuf (ai_text), the page's
   redraw after the box reading fonts through it too (v/fcache.c); as seen
   on an X16, "Question: s of text a page)." for a long selection */
static char aq[QMAX + 1];

/* (W72) the field's Polish letters. The kernel's GetString takes $20-$7F
   only (its key routine, StrKey, ignores a key with bit 7 set), so ą, ł,
   ż ... typed in the question's box were dropped. The box's DB_USR_ROUT
   (after DBGETSTRING: keyVector is StrKey then) puts a stub in front of
   StrKey; the stub is code in iobuf (main RAM: the kernel calls keyVector
   with the program's own bank mapped, not this overlay's; main RAM has no
   room for vr_tramp). A key below $A0 goes on to StrKey (a jump: Return
   ends the box from there, nothing of ours on the stack); $A0-$FF to
   qhigh, with this overlay's bank mapped */
static const unsigned char qstub[] = {
    0xAD, 0x49, 0x05,                   /*  0 lda keyData */
    0xC9, 0xA0,                         /*  3 cmp #$A0 */
    0xB0, 0x03,                         /*  5 bcs 10 */
    0x4C, 0, 0,                         /*  7 jmp StrKey (8: set by 24) */
    0xA5, 0x00,                         /* 10 lda $00 */
    0x48,                               /* 12 pha */
    0xA9, 0,                            /* 13 lda #bank (14) */
    0x85, 0x00,                         /* 15 sta $00 */
    0x20, 0, 0,                         /* 17 jsr qhigh (18) */
    0x68,                               /* 20 pla */
    0x85, 0x00,                         /* 21 sta $00 */
    0x60,                               /* 23 rts */
    0xAD, 0x2B, 0x05,                   /* 24 lda keyVector: the DB_USR_ROUT */
    0x8D, 0, 0,                         /* 27 sta 8 */
    0xAD, 0x2C, 0x05,                   /* 30 lda keyVector+1 */
    0x8D, 0, 0,                         /* 33 sta 9 */
    0xA9, 0,                            /* 36 lda #<stub */
    0x8D, 0x2B, 0x05,                   /* 38 sta keyVector */
    0xA9, 0,                            /* 41 lda #>stub */
    0x8D, 0x2C, 0x05,                   /* 43 sta keyVector+1 */
    0x60                                /* 46 rts */
};

/* a key of $A0-$FF in the field: StrKey given a space for it (its length,
   the 0 after the text, the fault at the margin, a beep when full), then
   the space made the letter (QB, its glyph, stringX: Backspace takes the
   letter's own width) */
static void qhigh(void)
{
    static unsigned char c, n, d;
    static unsigned int x0;
    c = keyData;
    n = strlen(QB);
    x0 = stringX;
    keyData = ' ';                      /* ($A0, Shift+Space: a space) */
    ((void (*)(void))(QK[8] | QK[9] << 8))();
    if (c != 0xA0 && strlen(QB) == n + 1) {
        /* (the kernel's entries called directly, veranda.inc's PromptOff,
           WPutChar, PromptOn: their C wrappers would take 32 bytes of main
           RAM, the test build's had 49) */
        asm("jsr $899E");               /* PromptOff */
        QB[n] = c;
        d = dispBufferOn;
        dispBufferOn = ST_WR_FORE;      /* (as GetString's: the foreground only) */
        r11 = x0;
        r12 = stringYW + baselineOffset;
        asm("lda %v", c);
        asm("jsr $8A27");               /* WPutChar: a, at (r11, r12); r11 past it */
        dispBufferOn = d;
        stringX = r11;
        asm("jsr $899B");               /* PromptOn */
    }
    keyData = c;
}

static unsigned char FASTCALL dlg(const unsigned char *t, unsigned char n)
{
    static unsigned char r;
    memcpy(DLG, t, n);
    if (t == qbox) {
        memcpy(QK, qstub, sizeof qstub);
        QK[14] = *(unsigned char *)0;   /* (this overlay's bank) */
        QK[18] = (unsigned char)VR_FN(qhigh);
        QK[19] = VR_FN(qhigh) >> 8;
        QK[28] = (unsigned char)((unsigned int)QK + 8);
        QK[29] = (unsigned int)(QK + 8) >> 8;
        QK[34] = (unsigned char)((unsigned int)QK + 9);
        QK[35] = (unsigned int)(QK + 9) >> 8;
        QK[37] = (unsigned char)(unsigned int)QK;
        QK[42] = (unsigned int)QK >> 8;
        DLG[23] = (unsigned char)((unsigned int)QK + 24);
        DLG[24] = (unsigned int)(QK + 24) >> 8;
    }
    r5 = (unsigned int)LN1;
    r6 = (unsigned int)LN2;
    r7 = (unsigned int)QB;
    QB[QMAX] = 0;
    view_cursor(0);
    r = DoDlgBox(DLG);
    strcpy(aq, QB);                     /* (QB[QMAX] 0; before the redraw below: it may use iobuf) */
    view_all();                         /* (a box this wide is not on the save-under stack: the
                                           kernel recovers it from the back screen, which left its
                                           text on the page; the text drawn again whole) */
    need_view = 1;
    status_dirty = 1;
    return r;
}

static void FASTCALL say(const char *a, const char *b)
{
    strcpy(LN1, a);
    strcpy(LN2, b);
    Beep();
    dlg(wbox, sizeof wbox);
}

/*---------------------------------------------------------------
  the files, in /VERANDA/.SYSTEM (the folder made current)
 ---------------------------------------------------------------*/
static unsigned char ah, ae;            /* AI.IN's stream; an error */

void FASTCALL plat_ai_put(const unsigned char *b, unsigned int n)
{
    ae |= FWrite(ah, (void *)b, n);
}

static unsigned char FASTCALL wopen(const char *name)
{
    strcpy(PATH, name);
    ah = FOpen(PATH, 'w');
    return ah >= 2;
}

/* iobuf's first n bytes into the file name: 0 done */
static unsigned char FASTCALL put(const char *name, unsigned int n)
{
    if (!wopen(name))
        return 1;
    ae = FWrite(ah, iobuf, n);
    return ae | FClose(ah);
}

static void FASTCALL del(const char *name)
{
    strcpy(PATH, name);
    FDelete(PATH);
}

/* the folder's AI.OUT and AI.RES deleted */
static void forget(void)
{
    vr_cd(VR_DATA);
    del("AI.OUT");
    del("AI.RES");
    vr_cdback();
}

/* (W70) a Veranda Ask aside (TaskCtl TASK_INFO, by its name) */
static unsigned char ask_aside(void)
{
    static unsigned char i;
    for (i = 0; i < TASK_MAX; ++i) {
        iobuf[TR_STATE] = 0;
        TaskCtl(TASK_INFO, i, iobuf);
        if (iobuf[TR_STATE] && !memcmp(iobuf + TR_NAME, "Veranda Ask", 12))
            return 1;
    }
    return 0;
}

/*---------------------------------------------------------------
  a request
 ---------------------------------------------------------------*/
static char outp[64];                   /* Veranda Ask's folder; AI.OUT's path */

static void FASTCALL request(unsigned char k)
{
    static unsigned char h, e;
    static pos_t lo, hi;
    /* Veranda Ask, in /VERANDA/APPS (or where the applications are) */
    strcpy(outp, vr_cd(VR_APPS));
    strcpy(PATH, "Veranda Ask.cvt");
    h = FOpen(PATH, 'r');
    if (h >= 2)
        FClose(h);
    vr_cdback();
    if (h < 2) {
        log("NO ASK");
        say("AI help needs Veranda Ask, which comes with Veranda", "(/VERANDA/APPS): it was not found.");
        return;
    }
    /* (W70) an earlier request still open (Veranda Ask came back without
       an answer: the user's Alt+Tab) and a Veranda Ask aside: that one
       holds it, on its screen, and Send there would answer it, not this
       one (as seen on an X16: another document's text shown as the text
       to send). Refused; the earlier request's range kept, so its answer
       still goes where it was asked */
    if (ai_st && ask_aside()) {
        log("ASK ASIDE");
        say("Veranda Ask still has the earlier request: switch to it", "(Alt+Tab), Send or Cancel it there, then ask again.");
        return;
    }
    if (HAS_SEL()) {
        ai_lo = SEL_LO();
        ai_hi = SEL_HI();
    } else
        ai_para(cur);                   /* (nothing selected: the paragraph at the cursor) */
    lo = ai_lo;
    hi = ai_hi;
    e = ai_check();
    if (e == AE_EMPTY) {
        log("EMPTY");
        set_status("AI help: no text to send");
        return;
    }
    if (e == AE_CELLS) {
        log("CELLS");
        say("AI help works on text in one cell of a table, or", "outside tables: select inside one cell.");
        return;
    }
    if (e == AE_LONG) {
        log("LONG");
        say("The text is too long for AI help: 8,192 characters", "at most. Select less of it.");
        return;
    }
    if (k == A_ASK) {
        strcpy(LN1, "Ask Veranda Ask about the text:");
        strcpy(LN2, "(it goes when you click Send in Veranda Ask)");
        QB[0] = 0;
        if (dlg(qbox, sizeof qbox) == CANCEL || !aq[0]) {
            log("NO QUESTION");
            return;
        }
        strcpy(tl, "QUESTION ");
        strcat(tl, aq);
        log(tl);
    }
    /* the files: AI.OUT and AI.RES of an earlier request first */
    vr_cd(VR_DATA);
    del("AI.OUT");
    del("AI.RES");
    e = 1;
    if (wopen("AI.IN")) {
        ae = 0;
        ai_text();
        e = ae | FClose(ah);
    }
    if (!e) {
        strcpy((char *)iobuf, "AIREQ 1\r\naction ");
        strcat((char *)iobuf, act[k]);
        if (k == A_ASK) {
            strcat((char *)iobuf, "\r\nquestion ");
            strcat((char *)iobuf, aq);  /* (W72: aq, not QB: iobuf held AI.IN's last piece) */
        }
        strcat((char *)iobuf, "\r\nback Veranda Write\r\n");
        e = put("AI.REQ", strlen((char *)iobuf));
    }
    if (!e) {
        strcpy((char *)iobuf, outp);    /* (Veranda Ask's folder) */
        if (iobuf[1])
            strcat((char *)iobuf, "/");
        strcat((char *)iobuf, "Veranda Ask\r\n");
        e = put("START.REQ", strlen((char *)iobuf));
    }
    if (e) {
        del("AI.IN");
        del("AI.REQ");
    }
    vr_cdback();
    if (e) {
        say("The card could not be written (/VERANDA/.SYSTEM):", "AI help was not asked.");
        return;
    }
    anc = lo;                           /* (what goes, selected) */
    cur = hi;
    ai_lo = lo;
    ai_hi = hi;
    ui_moved();
    ai_act = k;
    ai_gen0 = ai_gen;
    ai_st = 1;
    strcpy(tl, "ASIDE ");
    strcat(tl, act[k]);
    log(tl);
    set_status("Veranda Ask: check it, then Send");
    TaskCtl(TASK_ASIDE, 0, 0);
}

/*---------------------------------------------------------------
  the answer
 ---------------------------------------------------------------*/
static unsigned char piece[PIECE];

/* AI.OUT's bytes from at (up to PIECE) into piece: their count (the file
   opened for each, so no stream is open while the text changes, W12) */
static unsigned int FASTCALL rd_out(unsigned long at)
{
    static unsigned char h;
    static unsigned int n;
    strcpy(PATH, outp);
    h = FOpen(PATH, 'r');
    if (h < 2)
        return 0;
    n = 0;
    if (!FSeek(h, (unsigned int)at, (unsigned int)(at >> 16)))
        n = FRead(h, iobuf + AI_RUN, PIECE);
    FClose(h);
    if (n > PIECE)
        n = 0;
    memcpy(piece, iobuf + AI_RUN, n);
    return n;
}

static void apply(void)
{
    static unsigned long at;
    static unsigned int n;
    static unsigned char e, k;
    static const char *d;
    k = ai_act;
    d = vr_cd(VR_DATA);
    strcpy(outp, "/");
    if (d[1])
        strcat(outp, d);
    strcat(outp, "/:AI.OUT");
    vr_cdback();
    SetPointer(PTR_BUSY);
    ul_step(U_CMD, ai_hi, ai_lo);       /* (undo: the text and the selection as asked about) */
    ai_begin(k >= 5 ? AM_AFTER : AM_REPLACE);
    for (at = 0; (n = rd_out(at)) != 0; at += n)
        ai_add(piece, n);
    e = ai_end();
    SetPointer(PTR_ARROW);
    if (e == DE_AI) {
        log("EMPTY ANSWER");
        set_status("Veranda Ask's answer was empty");
        return;
    }
    if (e) {
        if (ul_undo()) {
            cur = ul_cur;
            anc = ul_anchor;
        }
        ui_moved();
        log("NO ROOM");
        say("There is not enough memory for the answer:", "the text was not changed.");
        return;
    }
    anc = ai_lo;
    cur = ai_hi;
    ui_moved();
    strcpy(tl, "OK ");
    strcat(tl, act[k]);
    log(tl);
    set_status(done[k]);
}

/* a line of AI.RES at p (CR or LF ends it): its length */
static unsigned char FASTCALL lnlen(const char *p)
{
    static unsigned char n;
    for (n = 0; p[n] && p[n] != '\r' && p[n] != '\n'; ++n)
        ;
    return n;
}

static void back(void)
{
    static unsigned char h, n, i;
    static char *p, *res, *msg;
    set_status("");                     /* (request()'s "check it, then Send" said its part) */
    vr_cd(VR_DATA);
    strcpy(PATH, "AI.RES");
    h = FOpen(PATH, 'r');
    n = 0;
    if (h >= 2) {
        n = (unsigned char)FRead(h, QB, 191);
        FClose(h);
    }
    vr_cdback();
    if (h < 2) {
        /* (W70) AI.REQ still there: Veranda Ask did not read the request (it
           did not start: too little memory?); the files deleted, so that a
           later start of Veranda Ask does not take this text for a request */
        vr_cd(VR_DATA);
        strcpy(PATH, "AI.REQ");
        h = FOpen(PATH, 'r');
        if (h >= 2) {
            FClose(h);
            del("AI.REQ");
            del("AI.IN");
        }
        vr_cdback();
        if (h >= 2) {
            ai_st = 0;
            log("NOT STARTED");
            say("Veranda Ask did not start (too little memory?): nothing", "was sent. Close a program, then ask again.");
            return;
        }
        ai_st = 3;                      /* (still open: looked at again when Write is back) */
        log("NOT YET");
        say("Veranda Ask has not answered yet. When it has, come back", "to Write, or choose AI > Check for the answer.");
        return;
    }
    QB[n] = 0;
    ai_st = 0;
    /* the lines: "AIRES 1", the result, "message <text>" */
    res = msg = 0;
    for (p = QB, i = 0; *p; ++i) {
        n = lnlen(p);
        if (p[n])
            p[n++] = 0;
        if (!i && strcmp(p, "AIRES 1"))
            break;
        if (i == 1)
            res = p;
        else if (i > 1 && !memcmp(p, "message ", 8))
            msg = p + 8;
        for (p += n; *p == '\r' || *p == '\n'; ++p)
            ;
    }
    if (!res) {
        log("BAD RESULT");
        say("Veranda Ask's answer could not be read:", "the text was not changed.");
    } else if (!strcmp(res, "ok")) {
        if (ai_gen != ai_gen0) {
            log("CHANGED");
            say("The text changed after AI help was asked: Veranda", "Ask's answer was not used. Ask again.");
        } else
            apply();
    } else if (!strcmp(res, "cancelled")) {
        log("CANCELLED");
        set_status("AI help cancelled");
    } else if (!strcmp(res, "error not-set-up")) {
        log("NOT SET UP");
        say("Veranda Ask is not signed in. Open Veranda Ask and sign in",
            "(or add an API key in its Settings).");
    } else {
        strcpy(tl, res);
        log(tl);
        if (!msg)
            msg = res;
        /* (its message in two lines, at a space before the 56th character) */
        n = strlen(msg);
        if (n > 56) {
            for (i = 56; i && msg[i] != ' '; --i)
                ;
            if (!i)
                i = 56;
            memcpy(tl, msg, i);
            tl[i] = 0;
            msg += i + (msg[i] == ' ');
        } else {
            strcpy(tl, msg);
            msg = "";
        }
        strcpy((char *)piece, msg);     /* (say's lines are iobuf's, and so is msg) */
        say(tl, (char *)piece);
    }
    forget();
}

/*---------------------------------------------------------------
  the AI menu's item t_arg (AI_BACK: Write is back after a request)
 ---------------------------------------------------------------*/
void ai_cmd(void)
{
    static unsigned char k;
    k = t_arg;
    if (k == A_ABOUT) {
        say("AI help is optional: Veranda Ask sends only the selected text,",
            "and only when you click Send there. Off: Tools > AI help.");
        return;
    }
    if (k == AI_BACK || k == A_CHECK) {
        if (ai_st)
            back();
        else
            set_status("No AI help asked for");
        return;
    }
    request(k);
}
