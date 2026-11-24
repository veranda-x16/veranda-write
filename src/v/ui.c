/* Veranda Write on Veranda: keys, the pointer, the commands (overlay 1,
   mapped for good as appBank: the kernel calls key, press and tick, and
   the menus' routines, here).

   A key is done at once (the text edited, the layout fixed: cheap, a
   line or two wrapped again); the drawing waits until no key is queued
   (tick), so typing at full speed is a few edits and one drawing.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include <theme.h>
#include "cwv.h"
#pragma code-name ("OVERLAY1")
#pragma rodata-name ("OVL1RO")         /* (its strings: copied before the kernel sees them) */

static unsigned char dragging;
#pragma rodata-name (push, "RODATA")
static const char running[] = "CWRITE RUNNING";   /* (main RAM: the kernel reads it) */
#pragma rodata-name (pop)

/*---------------------------------------------------------------
  moving
 ---------------------------------------------------------------*/
static void move_to(pos_t p, unsigned char sh)
{
    cur = pos_norm(p);
    if (!sh)
        anc = cur;
    ty_kept = 0;
    ui_moved();
}

static unsigned char space_at(pos_t p)
{
    unsigned char c = doc_byte(p);
    return c == ' ' || c == CH_TAB || c == CH_CR;
}

static void move_word(signed char d, unsigned char sh)
{
    pos_t p = cur;
    if (d > 0) {
        while (p < DOC_LEN && !space_at(p))
            p = pos_next(p);
        while (p < DOC_LEN && space_at(p))
            p = pos_next(p);
    } else {
        while (p && space_at(pos_prev(p)))
            p = pos_prev(p);
        while (p && !space_at(pos_prev(p)))
            p = pos_prev(p);
    }
    move_to(p, sh);
    goal_x = 0xFFFF;
}

static void move_line(int d, unsigned char sh)
{
    unsigned int L;
    lt_find(cur);
    L = lf_i;
    if (goal_x == 0xFFFF)
        goal_x = line_x(L, cur);
    if (d < 0)
        L = (unsigned int)-d > L ? 0 : L + d;
    else
        L = L + d >= lt_n ? lt_n - 1 : L + d;
    move_to(line_hit(L, goal_x), sh);
}

static void select_word(void)
{
    pos_t a = cur, b = cur;
    while (a && !space_at(pos_prev(a)))
        a = pos_prev(a);
    while (b < DOC_LEN && !space_at(b))
        b = pos_next(b);
    anc = a;
    cur = b;
    ui_moved();
}

void m_selall(void)
{
    GotoFirstMenu();
    anc = 0;
    cur = pos_norm(DOC_LEN);
    ui_moved();
    follow = 0;
}

/*---------------------------------------------------------------
  editing
 ---------------------------------------------------------------*/
static void no_room(void)
{
    if (doc_err == DE_MEM)
        message("There is not enough memory", "for that.");
    else if (doc_err == DE_STY)
        message("The document has too many styles", "(128 at most).");
    doc_err = 0;
}

static void del_sel(void)
{
    cur = anc = doc_delete(SEL_LO(), SEL_HI());
}

static void __fastcall__ set_named(unsigned char k);

static void type(unsigned char c)
{
    pos_t q;
    unsigned char k = 0;
    if (c == CH_CR && !HAS_SEL()) {
        q = par_start(cur);
        k = par[par_at(q)].kind;
        /* Return in an empty list item: the list ends (it becomes Body) */
        if (IS_LIST(k) && pos_norm(q) == cur && (cur >= DOC_LEN || doc_byte(cur) == CH_CR)) {
            set_named(PK_BODY);
            return;
        }
    }
    ul_step(U_TYPE, cur, anc);
    PM("ul");
    if (HAS_SEL()) {
        ty_style = style_typing(SEL_LO());
        del_sel();
    }
    cb_split = 1;                       /* (W43: a long tail in its chunk split off, not moved) */
    cur = anc = doc_insert(cur, &c, 1, ty_style);
    cb_split = 0;
    PM("ins");
    if (c == CH_CR) {
        ul_break();
        if (IS_HEAD(k) && !par_at(par_start(cur))) {
            t_arg = k;                  /* (a heading's end: Body's characters next) */
            CALL9(t_body);
        }
    }
    if (doc_err)
        no_room();
    goal_x = 0xFFFF;
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void backspace(void)
{
    pos_t p;
    ul_step(U_TYPE, cur, anc);
    if (HAS_SEL())
        del_sel();
    else if (cur) {
        p = pos_prev(cur);
        cur = anc = doc_delete(p, cur);
    }
    ty_kept = 0;
    ty_style = style_typing(cur);
    goal_x = 0xFFFF;
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void del_fwd(void)
{
    ul_step(U_TYPE, cur, anc);
    if (HAS_SEL())
        del_sel();
    else if (cur < DOC_LEN)
        cur = anc = doc_delete(cur, pos_next(cur));
    ty_kept = 0;
    ty_style = style_typing(cur);
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

void m_undo(void)
{
    GotoFirstMenu();
    if (ul_undo()) {
        cur = pos_norm(ul_cur);
        anc = pos_norm(ul_anchor);
        if (cur > DOC_LEN || anc > DOC_LEN)
            cur = anc = pos_norm(DOC_LEN);
        ui_moved();
    } else
        set_status("Nothing to undo");
    need_view = 1;
}

void m_redo(void)
{
    GotoFirstMenu();
    if (ul_redo()) {
        cur = anc = pos_norm(ul_cur > DOC_LEN ? DOC_LEN : ul_cur);
        ui_moved();
    } else
        set_status("Nothing to redo");
    need_view = 1;
}

void m_cut(void) { GotoFirstMenu(); callb(bank7, clip_cut); need_view = 1; follow = 1; }
void m_copy(void) { GotoFirstMenu(); callb(bank7, clip_copy); }
void m_paste(void) { GotoFirstMenu(); callb(bank7, clip_paste); need_view = 1; follow = 1; }

/* a style change: the selection's, or the typing style's */
void __fastcall__ apply_fmt(unsigned char op, unsigned char v)
{
    unsigned char back;
    if (HAS_SEL()) {
        back = cur < anc;
        ul_step(U_CMD, cur, anc);
        doc_format(SEL_LO(), SEL_HI(), op, v);
        if (doc_err)
            no_room();
        else if (back) {
            cur = tp_a;
            anc = tp_b;
        } else {
            anc = tp_a;
            cur = tp_b;
        }
        ty_style = style_typing(SEL_LO());
        ty_kept = 0;
    } else {
        v = fmt_map(ty_style, op, v);
        if (v == 0xFF) {
            doc_err = DE_STY;
            no_room();
            return;
        }
        ty_style = v;
        ty_kept = 1;
    }
    need_view = 1;
    status_dirty = 1;
}

static void toggle(unsigned char f)
{
    unsigned char on;
    GotoFirstMenu();
    if (HAS_SEL())
        on = doc_all_have(SEL_LO(), SEL_HI(), f);
    else
        on = (sty[ty_style].flags & f) != 0;
    apply_fmt(on ? FMT_CLR : FMT_SET, f);
}

void m_plain(void) { GotoFirstMenu(); apply_fmt(FMT_PLAIN, 0); }
void m_bold(void) { toggle(SF_BOLD); }
void m_italic(void) { toggle(SF_ITALIC); }
void m_uline(void) { toggle(SF_ULINE); }

/* the next size the font has, up or down */
static void step_size(signed char d)
{
    unsigned char i, s = sty[ty_style].size, n = 0;
    GotoFirstMenu();
    menu_choice = ty_style;
    CALL8(sizes_fill);                  /* (sz_list: the sizes of the style's font) */
    if (d > 0) {
        for (i = 0; i < sz_n; ++i)
            if (sz_list[i] > s) {
                n = sz_list[i];
                break;
            }
    } else
        for (i = sz_n; i--;)
            if (sz_list[i] < s) {
                n = sz_list[i];
                break;
            }
    if (n)
        apply_fmt(FMT_SIZE, n);
    else
        set_status(d > 0 ? "No larger size" : "No smaller size");
}

void m_larger(void) { step_size(1); }
void m_smaller(void) { step_size(-1); }

/* the Font and Size menus' items */
static void pick_font(unsigned char i)
{
    unsigned char f;
    f = font_index(fm_names[i]);
    if (f == 0xFF)
        message("The document has too many fonts", "(8 at most).");
    else
        apply_fmt(FMT_FONT, f);
}

static void pick_size(unsigned char i)
{
    apply_fmt(FMT_SIZE, sz_list[i]);
}

void m_about(void)
{
    GotoFirstMenu();
    message("Veranda Write 0.3 (stage 3): a word processor.", "MIT licence, (c) 2026 Marcin Spoczynski");
}

static void __fastcall__ set_named(unsigned char k)
{
    t_arg = k;
    CALL9(t_named);
}

static void __fastcall__ fmt_par(unsigned char i)
{
    t_arg = i;
    CALL9(t_format);
}


static void m_find(void) { GotoFirstMenu(); t_arg = 0; CALL9(fp_open); }
/* stage 3 (W44-W48): the page setup and the header and footer (a panel),
   a page break, PDF */
static void m_pagesetup(void) { GotoFirstMenu(); t_arg = 0; callb(bank16, pp_open); }
static void m_headfoot(void) { GotoFirstMenu(); t_arg = 5; callb(bank16, pp_open); }
static void m_pbreak(void) { GotoFirstMenu(); callb(bank14, t_pbreak); }
static void m_pdf(void) { GotoFirstMenu(); callb(bank16, t_pdf); }
static void m_replace(void) { GotoFirstMenu(); t_arg = 1; CALL9(fp_open); }
static void m_findnext(void) { GotoFirstMenu(); CALL9(fp_next); }
static void m_count(void) { GotoFirstMenu(); CALL9(t_count); }
static void m_goto(void) { GotoFirstMenu(); CALL9(t_goto); }
static void m_date(void) { GotoFirstMenu(); CALL9(t_date); }

/* the menus: a list made when its bar item opens (tools.c), each item's
   routine this one, with the item */
static void (*const acts[29])(void) = {
    m_about, m_count,
    c_new, c_open, c_save, c_saveas, m_pagesetup, m_pdf, c_close, c_quit,
    m_undo, m_redo, m_cut, m_copy, m_paste, m_selall, m_find, m_findnext, m_replace, m_goto,
    m_pbreak, m_date, m_headfoot,
    m_plain, m_bold, m_italic, m_uline, m_larger, m_smaller,
};

void __fastcall__ m_dyn(unsigned char bar)
{
    menu_bar = bar;
    CALL8(menu_make);
    r0 = (unsigned int)&msub;
}

void __fastcall__ m_item(unsigned char i)
{
    GotoFirstMenu();
    switch (menu_bar) {
    case 5: set_named(i); return;
    case 6: fmt_par(i); return;
    case 7: pick_font(i); return;
    case 8: pick_size(i); return;
    }
    acts[mfirst[menu_bar] + i]();
}

void c_new(void) { GotoFirstMenu(); CALL2(f_new); }
void c_open(void) { GotoFirstMenu(); CALL2(f_open); }
void c_save(void) { GotoFirstMenu(); CALL2(f_save); }
void c_saveas(void) { GotoFirstMenu(); CALL2(f_saveas); }
void c_close(void) { GotoFirstMenu(); CALL2(f_close); }
void c_quit(void) { GotoFirstMenu(); CALL2(f_quit); }

/*---------------------------------------------------------------
  keys
 ---------------------------------------------------------------*/
static void letter(unsigned char c, unsigned char sh)
{
    switch (c | 0x20) {
    case 'a': m_selall(); break;
    case 'c': if (sh) fmt_par(1); else m_copy(); break;
    case 'x': m_cut(); break;
    case 'v': m_paste(); break;
    case 'z': m_undo(); break;
    case 'y': m_redo(); break;
    case 'n': c_new(); break;
    case 'o': c_open(); break;
    case 's': if (sh) c_saveas(); else c_save(); break;
    case 'w': c_close(); break;
    case 'q': c_quit(); break;
    case 'r': if (sh) fmt_par(2); else m_replace(); break;
    case 'h': m_replace(); break;
    case 'f': m_find(); break;
    case 'g': if (sh) m_goto(); else m_findnext(); break;
    case 'd': if (sh) m_date(); break;
    case 'm': m_pbreak(); break;        /* (Ctrl+Return: Ctrl+M's code) */
    case 'p': if (sh) m_pdf(); break;
    case 'b': m_bold(); break;
    case 'i': m_italic(); break;
    case 'u': m_uline(); break;
    case 't': m_plain(); break;
    case 'e': if (sh) m_smaller(); else m_larger(); break;
    case 'l': fmt_par(0); break;
    case 'j': fmt_par(3); break;
    }
}

static void key(void)
{
    unsigned char k = keyData, m = keyDataMods, sh = m & 1, ctrl = m & 2;
    PM("k<");
    sw_ulin();                          /* (back from being put aside: the undo log's banks) */
    if (status_msg[0]) {
        status_msg[0] = 0;
        status_dirty = 1;
    }
#ifdef CW_TEST
    test_key(k);
#endif
    if (pp_on) {
        callb(bank16, pp_key);          /* (the page setup panel: every key its own) */
        return;
    }
    if (fp_on) {
        CALL9(fp_key);                  /* (the find panel's typing) */
        if (t_arg)
            return;
    }
    if (m & KEYM_ARROW) {
        switch (k) {
        case KEY_LEFT:
            if (ctrl)
                move_word(-1, sh);
            else if (HAS_SEL() && !sh)
                move_to(SEL_LO(), 0);
            else
                move_to(pos_prev(cur), sh);
            goal_x = 0xFFFF;
            break;
        case KEY_RIGHT:
            if (ctrl)
                move_word(1, sh);
            else if (HAS_SEL() && !sh)
                move_to(SEL_HI(), 0);
            else
                move_to(pos_next(cur), sh);
            goal_x = 0xFFFF;
            break;
        case KEY_UP:
            move_line(-1, sh);
            break;
        case KEY_DOWN:
            move_line(1, sh);
            break;
        }
        return;
    }
    if (ctrl && k >= 1 && k <= 26) {
        letter(k + 0x60, sh);
        return;
    }
    if (k >= 0x80 && (m & 4) && (k & 0x7F) >= 'A') {   /* Alt + a letter */
        letter(k & 0x7F, sh);
        return;
    }
    switch (k) {
    case KEY_HOME:
        lt_find(cur);
        move_to(lf_pos, sh);
        goal_x = 0;
        return;
    case KEY_CLEAR:                     /* Shift + Home */
        lt_find(cur);
        move_to(lf_pos, 1);
        goal_x = 0;
        return;
    case KEY_END:
        if (ctrl)
            move_to(DOC_LEN, sh);
        else {
            lt_find(cur);
            move_to(line_hit(lf_i, 0xFFFF), sh);
        }
        goal_x = 0xFFFF;
        return;
    case KEY_PGUP:
        if (ctrl)
            move_to(0, sh);
        else {
            view_scroll(-(int)(vn - 1));
            move_line(-(int)(vn - 1), sh);
        }
        return;
    case KEY_PGDN:
        view_scroll(vn - 1);
        move_line(vn - 1, sh);
        return;
    case 8:
        backspace();
        return;
    case KEY_DELETE:
        del_fwd();
        return;
    case CR:
    case 0x8D:
        type(CH_CR);
        return;
    case KEY_TAB:
        type(CH_TAB);
        return;
    case KEY_ESC:
        anc = cur;
        ui_moved();
        return;
    case 0xA0:
        if (sh)
            k = ' ';                    /* Shift + Space */
        break;
    }
    if ((k >= 32 && k < 127) || k >= 160)
        type(k);
    PM("key");
}

/*---------------------------------------------------------------
  the pointer
 ---------------------------------------------------------------*/
static void mouse_xy(unsigned int *x, unsigned int *y)
{
    do {
        *x = mouseXPos;
        *y = mouseYPosW;
    } while (*x != mouseXPos || *y != mouseYPosW);
}

static unsigned int dx, dy;             /* the pointer when drag last looked */

static unsigned char hit(unsigned int x, unsigned int y, pos_t *p)
{
    unsigned char r;
    if (y < TA_TOP || y > TA_BOT || x < PG_L || x > PG_R)
        return 0;
    r = view_row(y);
    if (r == 0xFF)
        r = vn - 1;
    *p = line_hit(top_line + r, x);
    return 1;
}

/* a toolbar pop-up (popup.c): the bar's menu m's list under the field
   at x, w wide; its item chosen as the menu's */
static void __fastcall__ popup(unsigned char m, unsigned int x, unsigned char w)
{
    t_arg = m;
    r11 = x;
    r12 = w;
    callb(bank13, t_popup);
    if (t_arg != 0xFF)
        m_item(t_arg);
    need_view = 1;
}

static void press(void)
{
    unsigned int x, y;
    pos_t p;
    unsigned char sh;
    if (mouseData & 0x80) {
        dragging = 0;
        return;
    }
    mouse_xy(&x, &y);
    if (pp_on) {
        r11 = x;                        /* (the page setup panel: the pointer its own) */
        r12 = y;
        callb(bank16, pp_press);
        return;
    }
    if (fp_on && y >= FP_Y && y <= TA_END && x >= PG_L && x <= PG_R) {
        r11 = x;
        r12 = y;
        CALL9(fp_press);
        return;
    }
    if (hit(x, y, &p)) {
        sh = GetKeyMods() & 1;
        if (!sh && p == cur && !HAS_SEL() && dblClickCount) {
            dblClickCount = 0;
            select_word();
            return;
        }
        move_to(p, sh);
        goal_x = 0xFFFF;
        dragging = 1;
        dx = x;
        dy = y;
        dblClickCount = 30;
    } else if (y >= TB_Y && y < TB_Y + 16) {
        /* the toolbar (16 pixels a button) */
        if (x >= TB_BIU && x < TB_BIU + 48)
            toggle(1 << ((x - TB_BIU) >> 4));
        else if (x >= TB_ALIGN && x < TB_ALIGN + 64)
            fmt_par((x - TB_ALIGN) >> 4);
        else if (x >= TB_LIST && x < TB_LIST + 32) {
            sh = PK_BULLET + ((x - TB_LIST) >> 4);
            set_named(par[par_at(par_start(cur))].kind == sh ? PK_BODY : sh);
        } else if (x >= TB_INDENT && x < TB_INDENT + 32)
            fmt_par(x < TB_INDENT + 16 ? 5 : 4);
        else if (x >= TB_FONT && x < TB_FONT + TB_FONTW)
            popup(7, TB_FONT, TB_FONTW);
        else if (x >= TB_SIZE && x < TB_SIZE + TB_SIZEW)
            popup(8, TB_SIZE, TB_SIZEW);
        else if (x >= TB_NAME && x < TB_NAME + TB_NAMEW)
            popup(5, TB_NAME, TB_NAMEW);
    } else if (x >= SB_X && y >= TA_TOP && y <= TA_END) {
        /* the scroll bar: the arrows three lines, the track a screen */
        if (y <= SB_UP)
            view_scroll(-3);
        else if (y >= SB_DN)
            view_scroll(3);
        else if (y < SB_UP + 1 + (sb_a << 3))
            view_scroll(-(int)(vn - 1));
        else if (y >= SB_UP + 1 + ((sb_a + sb_n) << 3))
            view_scroll(vn - 1);
        follow = 0;
    } else if (y >= 16 && y < 32 && x >= 616)
        c_close();                      /* the title band's close box */
}

static void drag(void)
{
    unsigned int x, y;
    pos_t p;
    if (mouseData & 0x80) {
        dragging = 0;
        return;
    }
    mouse_xy(&x, &y);
    if (y < TA_TOP) {
        move_line(-1, 1);
        return;
    }
    if (y > TA_BOT) {
        move_line(1, 1);
        return;
    }
    if (x < PG_L)
        x = PG_L;
    if (x > PG_R)
        x = PG_R;
    if (hit(x, y, &p) && p != cur) {
        cur = p;
        ui_moved();
        goal_x = 0xFFFF;
    }
}

/*---------------------------------------------------------------
  the main loop
 ---------------------------------------------------------------*/
static unsigned char pg_more = 1;
static unsigned char pend, pend_at;     /* keys not drawn yet: since cw_jiffies pend_at */

static void tick(void)
{
    signed char w;
    unsigned int y, x;
    dlg_keep();                         /* (back from being put aside: the banks again) */
    if (dragging)
        drag();
    asm("sei");
    w = (signed char)mouseWheel;
    mouseWheel = 0;
    asm("cli");
    if (w && menuNumber == 0) {
        view_scroll(3 * w);
        follow = 0;
    }
    mouse_xy(&x, &y);
    SetPointer(y >= TA_TOP && y <= TA_BOT && x >= PG_L && x <= PG_R ? PTR_IBEAM : PTR_ARROW);
    if ((pressFlag & SET_KEYPRESS) || cw_keys()) {
        /* draw once the keys waiting are in (W43: the ROM's too, which
           the kernel takes one a pass), or when the first of them came
           12 frames ago: typing faster than the drawing keeps up still
           shows on the screen, 5 times a second */
        cw_idle = 0;
        if (!pend) {
            pend = 1;
            pend_at = cw_jiffies();
        }
        if (!need_view || (unsigned char)(cw_jiffies() - pend_at) < 12)
            return;
    } else
        pend = 0;
    if (need_view) {
        pend = 0;
        /* (cw_idle: the main loop found nothing to do since the gate's
           player last looked; cleared only when there is work, so a
           frame that lands in the middle of an idle pass counts) */
        cw_idle = 0;
        need_view = 0;
        PM("t<");
        view_toolbar();
        PM("tbar");
        view_refresh();
        PM("ref");
        if (ld_shown) {
            ld_shown = 0;
            MARK("shown");              /* (the first screen of a document opened) */
        }
#ifdef CW_TEST
        test_drawn();
#endif
        return;
    }
    if (status_dirty) {
        /* the status bar in a pass of its own, after the text and the
           cursor, when no key is waiting (W43: 0.03 s a key) */
        cw_idle = 0;
        view_status();
#ifdef CW_TEST
        test_status();
#endif
        return;
    }
    if (ld_kind && menuNumber == 0) {
        cw_idle = 0;                    /* (the gate's player waits for all of it) */
        CALL2(f_ldstep);                /* the rest of a document being opened */
        return;
    }
    if (menuNumber == 0)
        view_blink();
    cw_idle = 1;
    if (pg_more || pq_k || pg_lo != 0xFFFF) {
        pg_more = pg_idle();            /* the pages counted on, in idle time */
        if (!pg_more)
            status_dirty = need_view = 1;
    }
#ifdef CW_TEST
    play_slow(cur_on);                  /* (the blink probe: tools/play.s) */
    test_tick();
#endif
}

void __fastcall__ ui_main(unsigned char opts)
{
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_home(opts);                      /* /DOCUMENTS first (Veranda's DECISIONS V71) */
    if (curDirName[0] != '/')
        strcpy((char *)curDirName, "/");
    longNames = LONG_NAMES;
    sw_init();                          /* the paged cbufs (the text, the line table) */
    bk_keep = 2;                        /* (banks left to Veranda: accessories, the kernel) */
    ul_init();
    callb(bank14, pg_loaddef);          /* (the user's page setup for new documents, W44) */
    doc_new();
    ty_style = 0;
    CALL8(menus_layout);
    view_frame();
#ifdef CW_TEST
    test_start();
#endif
    sw_start();                         /* the swap's folder (a stale one removed) */
    if ((opts & ST_LD_DATA) && dataFileName[0])
        CALL2(f_startdoc);
    theme_menus();                      /* the menu bar (Veranda's DECISIONS V74, V79) */
    DoMenu(&mbar, 0);
    keyVector = VR_FN(key);
    otherPressVec = VR_FN(press);
    appMain = VR_FN(tick);
    asideVector = VR_FN(cw_aside);
    need_view = 1;
    status_dirty = 1;
    follow = 1;
    LogText((void *)running);
}
