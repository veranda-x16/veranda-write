/* Veranda Write on Veranda: the menus' tables (main RAM, where the
   kernel reads them). The bar's items are dynamic: when one opens, its
   routine (m_dyn, overlay 1) has tools.c (overlay 8) make its list in
   msub, the one list table there is (only one is open at a time), its
   items' texts in itext; every item's routine is m_item (overlay 1),
   which the kernel calls with the item in A. So the menus take a table
   and a buffer of main RAM, whatever their number (stage 1's six tables
   and their texts took a kilobyte).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cwv.h"

struct menu mbar = { 0, 14, 0, 0, HORIZONTAL | UN_CONSTRAINED | NBAR, {
    { "Veranda Write", DYN_SUB_MENU, m_dyn }, { "File", DYN_SUB_MENU, m_dyn }, { "Edit", DYN_SUB_MENU, m_dyn },
    { "Insert", DYN_SUB_MENU, m_dyn }, { "Text", DYN_SUB_MENU, m_dyn }, { "Style", DYN_SUB_MENU, m_dyn }, { "Format", DYN_SUB_MENU, m_dyn },
    { "Font", DYN_SUB_MENU, m_dyn }, { "Size", DYN_SUB_MENU, m_dyn }, { "Tools", DYN_SUB_MENU, m_dyn } } };
struct menu msub;
char itext[MI][36];
unsigned int barx[NBAR];
unsigned char menu_bar;
unsigned char fm_n;
char fm_names[8][FONT_LEN];
unsigned char sz_n, sz_list[10];
unsigned char menu_choice;
unsigned char t_arg;
/* where each list's items start in tools.c's names and ui.c's routines */
const unsigned char mfirst[8] = { 0, 2, 12, 22, 25, 31, 38, 48 };
