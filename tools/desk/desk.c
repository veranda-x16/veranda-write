/* Veranda Write's gate: a test desktop (SHELL.cvt) for Alt+Tab. The
   switcher puts the program aside and starts the desktop; this one logs
   the tasks and brings the first one put aside back at once, as a click
   on its tile in Veranda's dock would (TaskCtl, Veranda's DECISIONS V83).
   With nothing put aside (the program quit: W60's session) and the
   session's second part there (CW.PL2 in the root), it starts Veranda
   Write again, which reads it.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <veranda.h>

static unsigned char rec[TASK_REC];
static char line[24] = "CW DESK TASK n STATE s";
static char app[] = "AUTOSTART";      /* (its file: a long name, without .cvt) */
static char pl2[] = "CW.PL2";

int main(void)
{
    unsigned char i, st;
    LogText("CW DESK");
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
