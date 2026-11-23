/* Veranda Write's gate: a test desktop (SHELL.cvt) for Alt+Tab. The
   switcher puts the program aside and starts the desktop; this one logs
   the tasks and brings the first one put aside back at once, as a click
   on its tile in Veranda's dock would (TaskCtl, Veranda's DECISIONS V83).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <veranda.h>

static unsigned char rec[TASK_REC];
static char line[24] = "CW DESK TASK n STATE s";

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
    return 0;
}
