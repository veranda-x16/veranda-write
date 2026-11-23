/* Veranda Write's unit tests: the host platform (host.c).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef HOST_H
#define HOST_H
extern unsigned int host_bank_limit;
void host_reset_banks(void);
extern unsigned int host_swap_max, host_swap_n;
extern const unsigned char *host_doc;
extern unsigned long host_doc_len, host_ios;
void host_swap_reset(void);
void host_freeze(void);
extern int host_fail_ok, host_fails, host_wfail;
#endif
