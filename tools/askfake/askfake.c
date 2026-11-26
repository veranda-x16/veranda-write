/* Veranda Write's gate (W68): a stand-in for Veranda Ask, installed as
   /VERANDA/APPS/Veranda Ask.cvt. The test desktop (tools/desk) starts it
   when START.REQ names Veranda Ask, as Veranda's desktop does (Veranda's
   V198); it plays Veranda Ask's part of the AI help request
   (Veranda's docs/AIREQ.md) without a network or a key: nothing is sent
   anywhere.

   The gate's script (tools/script.py) says what to answer: /AIFAKE, a
   line for each request in turn (/AIFAKE.N counts them):
     ok <text>             AI.OUT <text> ('|' a paragraph's end: CR),
                           AI.RES "AIRES 1", "ok", the action, ...
     cancelled             AI.RES "AIRES 1", "cancelled"
     error <code> <text>   AI.RES "AIRES 1", "error <code>", "message <text>"
     later <text>          as ok, but into AILATER.OUT and AILATER.RES: no
                           AI.RES yet (the user put Veranda Ask aside;
                           the test desktop renames them when Write is
                           next put aside)
     aside                 (W70) the request's files read and deleted, then
                           put aside (TaskCtl TASK_ASIDE; /ASKASIDE for the
                           test desktop); brought back, it answers its own
                           request: AI.OUT "ANSWER TO " and the first 64
                           bytes of its AI.IN, AI.RES ok
     nostart               (W70) the request's files left unread (Veranda
                           Ask not started), Write brought back
   For request k (A, B, ...) it copies AI.REQ to /AIREQ.k and AI.IN to
   /AIIN.k (the gate checks their bytes), deletes both (as Veranda Ask
   does), logs "ASKFAKE ..." and brings Veranda Write back (TaskCtl
   TASK_RESUME by its name, as Veranda Ask's "back" does).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <veranda.h>
#include <string.h>

static unsigned char buf[2100], rec[TASK_REC], st, k;
static char nm[24], nm2[24], to[12], lg[100], ctl[200], act[20];
static unsigned int n;

static unsigned char __fastcall__ put(const char *name, const void *b, unsigned int len)
{
    unsigned char h;
    strcpy(nm, name);
    h = FOpen(nm, 'w');
    if (h < 2)
        return 1;
    FWrite(h, (void *)b, len);
    return FClose(h);
}

/* the file name (in the folder now) copied to the root's to; its bytes */
static unsigned int __fastcall__ copy(const char *name, const char *to)
{
    unsigned char h, w;
    unsigned int m, t = 0;
    strcpy(nm, name);
    h = FOpen(nm, 'r');
    if (h < 2)
        return 0xFFFF;
    strcpy(nm2, "//:");
    strcat(nm2, to);
    w = FOpen(nm2, 'w');
    while ((m = FRead(h, buf, 512)) != 0 && m <= 512) {
        if (w >= 2)
            FWrite(w, buf, m);
        t += m;
    }
    FClose(h);
    if (w >= 2)
        FClose(w);
    return t;
}

static void __fastcall__ logn(const char *a, unsigned int v)
{
    char *p;
    strcpy(lg, a);
    p = lg + strlen(lg);
    *p++ = '0' + (char)(v / 10000 % 10);
    *p++ = '0' + (char)(v / 1000 % 10);
    *p++ = '0' + (char)(v / 100 % 10);
    *p++ = '0' + (char)(v / 10 % 10);
    *p++ = '0' + (char)(v % 10);
    *p = 0;
    LogText(lg);
}

static void back_to_write(void);
static unsigned char held[64];          /* ("aside": the first bytes of its own request's AI.IN) */
static unsigned int heldn;

static void tick(void)
{
    unsigned char h, i, line;
    char *p, *q;
    if (st == 2) {                      /* ("aside": brought back; it answers its own request) */
        st = 3;
        LogText("ASKFAKE ASIDE BACK");
        ChDir(VR_DATADIR);
        memcpy(buf, "ANSWER TO ", 10);
        memcpy(buf + 10, held, heldn);
        put("AI.OUT", buf, 10 + heldn);
        strcpy(lg, "AIRES 1\r\nok\r\naction rewrite\r\nservice plan\r\nmodel fake\r\nbytes 0000\r\n");
        put("AI.RES", lg, strlen(lg));
        back_to_write();
        return;
    }
    if (st)
        return;
    st = 1;
    /* which request this is; its line of /AIFAKE */
    ChDir("/");
    k = 0;
    strcpy(nm, "AIFAKE.N");
    h = FOpen(nm, 'r');
    if (h >= 2) {
        FRead(h, &k, 1);
        FClose(h);
    }
    i = k + 1;
    put("AIFAKE.N", &i, 1);
    strcpy(nm, "AIFAKE");
    h = FOpen(nm, 'r');
    n = 0;
    if (h >= 2) {
        n = FRead(h, buf, 2048);
        FClose(h);
    }
    if (n > 2048)
        n = 0;
    buf[n] = 0;
    ctl[0] = 0;
    for (p = (char *)buf, line = 0; *p; ++line) {
        for (q = p; *q && *q != '\n'; ++q)
            ;
        if (line == k) {
            memcpy(ctl, p, q - p < 199 ? q - p : 199);
            ctl[q - p < 199 ? q - p : 199] = 0;
        }
        p = *q ? q + 1 : q;
    }
    strcpy(lg, "ASKFAKE REQUEST ");
    lg[16] = 'A' + k;
    lg[17] = ' ';
    strcpy(lg + 18, ctl);
    lg[60] = 0;
    LogText(lg);
    /* (W70) "nostart": as if Veranda Ask had not started (too little
       memory): the request's files left as they are, Write back */
    if (!memcmp(ctl, "nostart", 7)) {
        back_to_write();
        return;
    }
    /* the request's files: copied for the gate, deleted */
    ChDir(VR_DATADIR);
    strcpy(to, "AIREQ.A");
    to[6] = 'A' + k;
    logn("ASKFAKE AI.REQ ", copy("AI.REQ", to));
    /* the action, for AI.RES */
    act[0] = 0;
    strcpy(nm, "AI.REQ");
    h = FOpen(nm, 'r');
    if (h >= 2) {
        n = FRead(h, buf, 255);
        FClose(h);
        buf[n < 256 ? n : 0] = 0;
        p = strstr((char *)buf, "action ");
        if (p) {
            for (p += 7, i = 0; *p > ' ' && i < 19; )
                act[i++] = *p++;
            act[i] = 0;
        }
    }
    strcpy(to, "AIIN.A");
    to[5] = 'A' + k;
    logn("ASKFAKE AI.IN ", copy("AI.IN", to));
    heldn = 0;
    strcpy(nm, "AI.IN");
    h = FOpen(nm, 'r');
    if (h >= 2) {
        heldn = FRead(h, held, sizeof held);
        FClose(h);
        if (heldn > sizeof held)
            heldn = 0;
    }
    strcpy(nm, "AI.REQ");
    FDelete(nm);
    strcpy(nm, "AI.IN");
    FDelete(nm);
    /* (W70) "aside": Veranda Ask put aside with the request on its screen,
       unanswered (the user's Alt+Tab); brought back later, it answers the
       request it holds, whatever the files say then */
    if (!memcmp(ctl, "aside", 5)) {
        LogText("ASKFAKE ASIDE");
        st = 2;
        ChDir("/");
        i = 1;
        put("ASKASIDE", &i, 1);         /* (the test desktop: Write back first, then this) */
        TaskCtl(TASK_ASIDE, 0, 0);
        return;
    }
    /* the answer */
    if (!memcmp(ctl, "ok ", 3) || !memcmp(ctl, "later ", 6)) {
        p = ctl + (ctl[0] == 'o' ? 3 : 6);
        for (n = 0; p[n]; ++n)
            buf[n] = p[n] == '|' ? '\r' : p[n];
        put(ctl[0] == 'o' ? "AI.OUT" : "AILATER.OUT", buf, n);
        strcpy(lg, "AIRES 1\r\nok\r\naction ");
        strcat(lg, act);
        strcat(lg, "\r\nservice plan\r\nmodel fake\r\nbytes ");
        p = lg + strlen(lg);
        *p++ = '0' + (char)(n / 1000 % 10);
        *p++ = '0' + (char)(n / 100 % 10);
        *p++ = '0' + (char)(n / 10 % 10);
        *p++ = '0' + (char)(n % 10);
        strcpy(p, "\r\n");
        put(ctl[0] == 'o' ? "AI.RES" : "AILATER.RES", lg, strlen(lg));
    } else if (!memcmp(ctl, "cancelled", 9)) {
        put("AI.RES", "AIRES 1\r\ncancelled\r\n", 20);
    } else if (!memcmp(ctl, "error ", 6)) {
        p = strchr(ctl + 6, ' ');
        if (p)
            *p++ = 0;
        strcpy(lg, "AIRES 1\r\nerror ");
        strcat(lg, ctl + 6);
        if (p) {
            strcat(lg, "\r\nmessage ");
            strcat(lg, p);
        }
        strcat(lg, "\r\n");
        put("AI.RES", lg, strlen(lg));
    }
    back_to_write();
}

/* back: Veranda Write, aside */
static void back_to_write(void)
{
    unsigned char i;
    for (i = 0; i < TASK_MAX; ++i) {
        TaskCtl(TASK_INFO, i, rec);
        if (!rec[TR_STATE])
            continue;
        rec[TR_NAME + 16] = 0;
        strcpy(lg, "ASKFAKE TASK ");
        strcat(lg, (char *)rec + TR_NAME);
        LogText(lg);
        /* (the gate starts the test build as AUTOSTART: its name there) */
        if (!memcmp((char *)rec + TR_NAME, "Veranda Write", 13) || !strcmp((char *)rec + TR_NAME, "AUTOSTART")) {
            LogText("ASKFAKE BACK");
            TaskCtl(TASK_RESUME, i, 0);
        }
    }
    LogText("ASKFAKE NO WRITE");
}

int main(void)
{
    LogText("ASKFAKE STARTED");
    appMain = (unsigned int)tick;
    return 0;
}
