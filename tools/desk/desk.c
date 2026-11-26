/* Veranda Write's gate: a test desktop (SHELL.cvt) for Alt+Tab. The
   switcher puts the program aside and starts the desktop; this one logs
   the tasks and brings the first one put aside back at once, as a click
   on its tile in Veranda's dock would (TaskCtl, Veranda's DECISIONS V83).
   With nothing put aside (the program quit: W60's session) and the
   session's second part there (CW.PL2 in the root), it starts Veranda
   Write again, which reads it.

   (W68) When START.REQ in /VERANDA/.SYSTEM names Veranda Ask (AI help's
   request, Veranda's docs/AIREQ.md), it is deleted and Veranda Ask is
   started from /VERANDA/APPS as Veranda's desktop does (the gate's is a
   stand-in, tools/askfake); a START.REQ for another program is left as
   it is (W61's gate reads it). A staged answer (AILATER.OUT, AILATER.RES:
   the stand-in's "later", Veranda Ask put aside) becomes AI.OUT and
   AI.RES, as if Veranda Ask had answered meanwhile. (W70) The stand-in
   put aside with a request (/ASKASIDE): Write back first, then the
   stand-in.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <veranda.h>
#include <string.h>

static unsigned char rec[TASK_REC];
static char line[24] = "CW DESK TASK n STATE s";
static char app[] = "AUTOSTART";      /* (its file: a long name, without .cvt) */
static char pl2[] = "CW.PL2";
static char req[40], nm[16], nm2[16];
static const char ask[] = "/VERANDA/APPS/Veranda Ask";

static void __fastcall__ ren(const char *a, const char *b)
{
    strcpy(nm, a);
    strcpy(nm2, b);
    FDelete(nm2);
    FRename(nm, nm2);
}

/* (W70) the task of that name brought back (else nothing) */
static void __fastcall__ resume(const char *name)
{
    unsigned char i;
    for (i = 0; i < TASK_MAX; ++i) {
        rec[TR_STATE] = 0;
        TaskCtl(TASK_INFO, i, rec);
        if (rec[TR_STATE] && !strcmp((char *)rec + TR_NAME, name)) {
            LogText(name == app ? "CW DESK RESUME WRITE" : "CW DESK RESUME ASK");
            TaskCtl(TASK_RESUME, i, 0);
        }
    }
}

int main(void)
{
    unsigned char i, st, n;
    LogText("CW DESK");
    ChDir(VR_DATADIR);
    strcpy(nm, "AILATER.RES");
    st = FOpen(nm, 'r');
    if (st >= 2) {
        FClose(st);
        LogText("CW DESK LATER");
        ren("AILATER.OUT", "AI.OUT");
        ren("AILATER.RES", "AI.RES");
    }
    strcpy(nm, "START.REQ");
    st = FOpen(nm, 'r');
    n = 0;
    if (st >= 2) {
        n = (unsigned char)FRead(st, req, sizeof req - 1);
        FClose(st);
    }
    if (n >= sizeof ask - 1 && n < sizeof req && !memcmp(req, ask, sizeof ask - 1)) {
        FDelete(nm);
        LogText("CW DESK ASK");
        ChDir("/VERANDA/APPS");
        longNames = LONG_NAMES;
        GetFile((void *)"Veranda Ask", 0, 0, 0, 0);
        LogText("CW DESK NO ASK");
    }
    /* (W70) the stand-in for Veranda Ask put itself aside with a request
       (its "aside": /ASKASIDE, 1): Veranda Write is brought back first (the
       user's click on its tile), then, the next time (2: Write put aside
       again by Alt+Tab), Veranda Ask */
    ChDir("/");
    strcpy(nm, "ASKASIDE");
    st = FOpen(nm, 'r');
    n = 0;
    if (st >= 2) {
        FRead(st, &n, 1);
        FClose(st);
        FDelete(nm);
    }
    if (n == 1) {
        n = 2;
        st = FOpen(nm, 'w');
        if (st >= 2) {
            FWrite(st, &n, 1);
            FClose(st);
        }
        resume(app);
    } else if (n == 2)
        resume("Veranda Ask");
    for (i = 0; i < TASK_MAX; ++i) {
        rec[TR_STATE] = 0xEE;
        st = TaskCtl(TASK_INFO, i, rec);
        line[13] = '0' + i;
        line[21] = '0' + (rec[TR_STATE] & 15);
        LogText(line);
        if (rec[TR_STATE] && rec[TR_STATE] != 0xEE) {
            LogText("CW DESK RESUME");
            TaskCtl(TASK_RESUME, i, 0);
        }
    }
    LogText("CW DESK NONE");
    ChDir("/");
    st = FOpen(pl2, 'r');
    if (st >= 2) {
        FClose(st);
        LogText("CW DESK RESTART");
        longNames = LONG_NAMES;
        GetFile(app, 0, 0, 0, 0);
        LogText("CW DESK NO RESTART");
    }
    return 0;
}
