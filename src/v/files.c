/* Veranda Write on Veranda: overlay 2: the document's files.

   A document is a plain file on the card, any long name (FAT, up to 63
   characters; ".cwr" added when the name has no extension), in the
   format of docs/FORMAT.md (format.c). Open takes a Veranda Write
   document, or any other file as plain text (format.c's fmt_text).
   Saving writes ~CWRITE.TMP, then the old file goes and the new one takes
   its name, so a failed write leaves the old one there. Nothing stays
   open between commands (the switcher puts aside a program with no
   streams open: Veranda's DECISIONS V83).

   A document is read a step at a time (DECISIONS W30): its first screen
   at Open, the rest from the main loop (f_ldstep), each step the file
   opened again where the last one ended. The pager reads the text it
   has put out of RAM back from the document's file (sw_doc: which file
   that is; after a save, the new one).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrchoose.h>
#include <vrpath.h>
#include "cwv.h"
#include "../spell.h"
#include "view.h"
#include "../office.h"
#pragma code-name ("OVERLAY2")
#pragma rodata-name ("OVL2RO")         /* (its strings: copied before the kernel sees them) */

static char tmpname[] = "~CWRITE.TMP";   /* (main RAM: the kernel reads it) */
static char nm[68];                     /* the file being opened */
static unsigned char fh, ferr;
unsigned char ld_shown;                 /* (the test build: the first screen after an open to time) */

static unsigned int rd(void *p, unsigned int n)
{
    unsigned int k = FRead(fh, p, n);
    return k;
}

static unsigned char wr(const void *p, unsigned int n)
{
    if (FWrite(fh, (void *)p, n))
        ferr = 1;
    return ferr;
}

static void doc_dir(void)
{
    ChDir((char *)curDirName);
}

static void title(void)
{
    status_dirty = 1;
    need_view = 1;
    view_title();
}

static void fresh(void)
{
    cur = anc = 0;
    top_line = 0;
    ty_style = 0;
    ty_kept = 0;
    goal_x = 0xFFFF;
    title();
    view_all();
}

/* a step, the first one: the first screen's worth (DECISIONS W30) */
#define LD_FIRST 24576UL
#define LD_STEP  65536UL

static void ld_fail(unsigned char e)
{
#ifdef CW_TEST
    static char t[] = "CW LDFAIL x";
    t[10] = '0' + e;
    LogText(t);
#endif
    doc_new();
    docname[0] = 0;
    fresh();
    message(e == FE_MEM ? "There is not enough memory for" : e == FE_NEWER ? "A newer Veranda Write made"
            : e == FE_BIG ? "More than 4 MB of text is in" : e == FE_LINES ? "More than 65,000 lines are in"
            : "Cannot read", nm);
}

/* (format.c, between buffers) input waiting: the step ends */
static void poll(void)
{
    if (pressFlag & (SET_KEYPRESS | SET_MOUSE))
        ld_stop = 1;
}

/* name: a Veranda Write document, a geoWrite document (converted,
   when asked: it opens as Untitled-name, the original kept: W26) or
   plain text; 0 done. A Veranda Write document or plain text: its
   first screen read now, the rest a step at a time from the main loop
   (f_ldstep, W30); a geoWrite document is converted whole (its text,
   new, goes to the swap when it is larger than RAM) */
static unsigned char load(const char *name)
{
    static unsigned char kind, e;
    static unsigned int n;
    static char *p;
    callb(bank16, sp_unload);           /* (W59: spelling's banks back: a .docx or an .odt is converted in RAM) */
    strncpy(nm, name, 63);
    nm[63] = 0;
    fh = FOpen(nm, 'r');
    if (fh < 2 && strlen(nm) < 60) {
        strcat(nm, ".cvt");             /* (the shell names a GEOS document without it) */
        fh = FOpen(nm, 'r');
        if (fh < 2)
            nm[strlen(nm) - 4] = 0;
    }
    if (fh < 2) {
        message("Cannot open", nm);
        return 1;
    }
    n = FRead(fh, iobuf, 508);
    kind = n >= 4 && !memcmp(iobuf, CW_MAGIC, 4) ? 1 : n == 508 ? gw_check(iobuf) << 1 : 0;
    if (!kind && n >= 4 && !memcmp(iobuf, "PK\3\4", 4))
        kind = 6;                       /* (a ZIP: a .docx or an .odt, W49) */
    FClose(fh);
    if (kind == 2) {
        message("Veranda Write cannot open this GEOS file:", nm);
        return 1;
    }
    if (kind == 4 && !yesno("Convert this geoWrite file?", "The original is kept."))
        return 1;
    MARK("open<");
    fh = FOpen(nm, 'r');
    view_cursor(0);
    sw_doc(nm);                         /* (the text read back from it: the pager) */
    lay_busy = busy_ptr;                /* (main RAM: layfix.c calls it from overlay 4) */
    fmt_cursor = 0;
    if (kind == 4)
        e = gw_load(fh);
    else if (kind == 6) {
        t_arg = 0;                      /* (an import's) */
        callb(bank16, ox_up);           /* (W66: the office overlays, from SPELL.OVL) */
        e = t_arg ? ox_load(fh) : FE_IO;
    } else {
        e = kind ? fmt_begin(rd) : fmt_tbegin(rd);
        if (!e && ld_kind)
            e = fmt_step(rd, LD_FIRST);
    }
    lay_busy = 0;
    FClose(fh);
    docname[1] = 0;
    if (e) {
        callb(bank16, ox_down);
        ld_fail(e);
        return 1;
    }
    strcpy(docname, nm);
    if (kind != 1)
        docname[0] = 0;                 /* (saved as a document by Save as) */
    if (kind >= 4) {
        /* Untitled-name: Save as offers name.cwr (W27, W51) */
        if ((p = strrchr(nm, '.')) != 0)
            *p = 0;                     /* (.cvt, .docx, .odt) */
        nm[50] = 0;
        strcpy(docname + 1, nm);
    }
    fresh();
    cur = anc = fmt_cursor <= DOC_LEN ? pos_norm(fmt_cursor) : 0;
    ld_shown = 1;
    ui_moved();
    if (kind == 4)
        gw_note();
    else if (kind == 6)
        ox_note();                      /* (then the office overlays' banks back) */
    else if (!ld_kind)
        l1_note();                      /* (W38: letters not in the set, said) */
    return 0;
}

/* the main loop's: a step more of the document being opened (from where
   the last one ended: the file opened again, W12) */
void f_ldstep(void)
{
    unsigned char e;
    if (!ld_kind)
        return;
    fh = sw_dopen(ld_at);
    if (fh < 2) {
        ld_fail(FE_IO);
        return;
    }
    ld_poll = poll;
    e = fmt_step(rd, LD_STEP);
    ld_poll = 0;
    sw_dclose(fh);
    if (e) {
        ld_fail(e);
        return;
    }
    /* (what came in is below the screen: nothing to draw; the scroll bar
       at the end) */
    if (ld_kind && vn && dm_lo >= vr[vn - 1].pos + vr[vn - 1].l.len)
        dm_on = 0;
    if (!ld_kind)
        l1_note();                      /* (W38: read whole: what the set had not) */
    if (!ld_kind && fmt_cursor <= DOC_LEN && !cur && !anc && fmt_cursor) {
        cur = anc = pos_norm(fmt_cursor);   /* (the cursor kept in the file: now in) */
        ui_moved();
    }
    need_view = 1;
    status_dirty = 1;
}

/* an edit up to p (0xFFFFFFFF: one that needs all of it) while the
   document is being opened: the rest read first, unless the edit is
   well inside what is in (format.c's ld_safe) */
void __fastcall__ e_ld_need(pos_t p)
{
    if (!ld_kind || ld_safe(p))
        return;
    SetPointer(PTR_BUSY);
    while (ld_kind)
        f_ldstep();
    SetPointer(PTR_ARROW);
}

static unsigned char save(void)
{
    static const char *why;
    if (ld_kind)
        e_ld_need(0xFFFFFFFFUL);        /* (all of it in first) */
    doc_dir();
    fh = FOpen((char *)tmpname, 'w');
    if (fh < 2) {
        message("Cannot write in this folder:", (char *)curDirName);
        return 1;
    }
    ferr = 0;
    SetPointer(PTR_BUSY);
    if (fmt_save(wr, cur))
        ferr = 1;
    if (FClose(fh))
        ferr = 1;
    if (ferr) {
        FDelete((char *)tmpname);
        why = "(is the card full?)";
        message("The document could not be written", why);
        return 1;
    }
    FDelete(docname);
    if (FRename((char *)tmpname, docname)) {
        sw_doc(tmpname);                /* (the text is that file's now) */
        sw_saved(fmt_toff);
        message("Saved as ~CWRITE.TMP: cannot rename it to", docname);
        return 1;
    }
    /* the text is the new file's: what the pager reads back from */
    sw_doc(docname);
    sw_saved(fmt_toff);
    doc_modified = 0;
    set_status("Saved");
    title();
    return 0;
}

/* the changes kept or dropped first: 1 go on, 0 stop */
static unsigned char ask_discard(void)
{
    static char b[64];
    unsigned char r;
    if (!doc_modified)
        return 1;
    strncpy(b, docname[0] ? docname : "Untitled", sizeof b - 2);
    strcat(b, "?");
    r = yesnocancel("Save the changes to", b);
    if (r == NO)
        return 1;
    if (r != YES)
        return 0;
    if (!docname[0]) {
        f_saveas();
        return !doc_modified;
    }
    return !save();
}

void f_new(void)
{
    if (!ask_discard())
        return;
    doc_new();
    docname[0] = docname[1] = 0;
    fresh();
}

void f_close(void)
{
    f_new();
}

void f_quit(void)
{
    if (ask_discard()) {
        sw_end();                       /* (the swap's files) */
        EnterDeskTop();
    }
}

/* (v/tbui.c's New from template, W67) the changes saved or not first:
   t_arg 1 go on; the template pdf_name in curDirName opened as a new
   document (Untitled: its file only read), t_arg 1 */
void f_discard(void)
{
    t_arg = ask_discard();
}

void f_tload(void)
{
    extern char pdf_name[64];
    doc_dir();
    t_arg = !load(pdf_name);
    if (t_arg) {
        docname[0] = docname[1] = 0;
        title();
    }
}

void f_startdoc(void)
{
    doc_dir();
    load((char *)dataLongName);
}

void f_open(void)
{
    static char name[64];
    unsigned char h;
    if (!ask_discard())
        return;
    name[0] = 0;
    doc_dir();
    gw_scan();                          /* (the folder's geoWrite documents: the dialog lists them) */
    vr_file_filter = gw_filter;         /* (glue.s's: main RAM) */
    h = file_dlg(VRF_OPEN, "Open a document", name);
    vr_file_filter = 0;
    if (h)
        load(name);
}

void f_save(void)
{
    if (!docname[0])
        f_saveas();
    else
        save();
}

void f_saveas(void)
{
    static char name[64];
    unsigned char h;
    if (docname[0])
        strcpy(name, docname);
    else if (docname[1])
        strcat(strcpy(name, docname + 1), ".cwr");   /* (a geoWrite document converted) */
    else
        strcpy(name, "Untitled.cwr");
    for (;;) {
        if (!file_dlg(VRF_SAVE, "Save the document as", name))
            return;
        if (!strchr(name, '.') && strlen(name) < 59)
            strcat(name, ".cwr");
        h = FOpen(name, 'r');
        if (h < 2)
            break;
        FClose(h);
        if (yesno("Replace the file", name))
            break;
    }
    strcpy(docname, name);
    save();
}
