/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.2 (Berkeley) 4/18/94
 * $FreeBSD$
 */

#ifndef _PAX_EXTERN_H_
#define _PAX_EXTERN_H_

/*
 * External references from each source file
 */

#include <sys/cdefs.h>
#ifdef __APPLE__
#include <signal.h>
#endif /* __APPLE__ */

/*
 * ar_io.c
 */
extern const char *arcname;
extern const char *gzip_program;
int ar_open(const char *);
void ar_close(void);
void ar_drain(void);
int ar_set_wr(void);
int ar_app_ok(void);
int ar_read(char *, int);
int ar_write(char *, int);
int ar_rdsync(void);
int ar_fow(off_t, off_t *);
int ar_rev(off_t );
int ar_next(void);

/*
 * ar_subs.c
 */
extern u_long flcnt;
#ifdef __APPLE__
int updatepath(void);
int dochdir(const char *);
int fdochdir(int);
#endif /* __APPLE__ */
void list(void);
void extract(void);
void append(void);
void archive(void);
void copy(void);

/*
 * buf_subs.c
 */
extern int blksz;
extern int wrblksz;
extern int maxflt;
extern int rdblksz;
extern off_t wrlimit;
extern off_t rdcnt;
extern off_t wrcnt;
int wr_start(void);
int rd_start(void);
void cp_start(void);
int appnd_start(off_t);
int rd_sync(void);
void pback(char *, int);
int rd_skip(off_t);
void wr_fin(void);
int wr_rdbuf(char *, int);
int rd_wrbuf(char *, int);
int wr_skip(off_t);
int wr_rdfile(ARCHD *, int, off_t *);
int rd_wrfile(ARCHD *, int, off_t *);
void cp_file(ARCHD *, int, int);
int buf_fill(void);
int buf_flush(int);

/*
 * cache.c
 */
int uidtb_start(void);
int gidtb_start(void);
int usrtb_start(void);
int grptb_start(void);
const char * name_uid(uid_t, int);
const char * name_gid(gid_t, int);
int uid_name(char *, uid_t *);
int gid_name(char *, gid_t *);

/*
 * cpio.c
 */
int cpio_strd(void);
int cpio_trail(ARCHD *);
int cpio_endwr(void);
int cpio_id(char *, int);
int cpio_rd(ARCHD *, char *);
off_t cpio_endrd(void);
int cpio_stwr(void);
int cpio_wr(ARCHD *);
int vcpio_id(char *, int);
int crc_id(char *, int);
int crc_strd(void);
int vcpio_rd(ARCHD *, char *);
off_t vcpio_endrd(void);
int crc_stwr(void);
int vcpio_wr(ARCHD *);
int bcpio_id(char *, int);
int bcpio_rd(ARCHD *, char *);
off_t bcpio_endrd(void);
int bcpio_wr(ARCHD *);

/*
 * file_subs.c
 */
#ifdef __APPLE__
extern char *gnu_name_string, *gnu_link_string;
#endif /* __APPLE__ */
int file_creat(ARCHD *);
void file_close(ARCHD *, int);
int lnk_creat(ARCHD *);
int cross_lnk(ARCHD *);
int chk_same(ARCHD *);
int node_creat(ARCHD *);
int unlnk_exist(char *, int);
#ifdef __APPLE__
int chk_path(char *, uid_t, gid_t, char **);
void set_ftime(char *, time_t, time_t, time_t, time_t, int);
#else
int chk_path(char *, uid_t, gid_t);
void set_ftime(char *fnm, time_t mtime, time_t atime, int frc);
#endif /* __APPLE__ */
int set_ids(char *, uid_t, gid_t);
int set_lids(char *, uid_t, gid_t);
void set_pmode(char *, mode_t);
int file_write(int, char *, int, int *, int *, int, char *);
void file_flush(int, char *, int);
void rdfile_close(ARCHD *, int *);
int set_crc(ARCHD *, int);

#ifdef __APPLE__
/*
 * for set_ftime()
 */
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
#define st_atime_sec    st_atimespec.tv_sec
#define st_atime_nsec   st_atimespec.tv_nsec
#define st_mtime_sec    st_mtimespec.tv_sec
#define st_mtime_nsec   st_mtimespec.tv_nsec
#else
#define st_atime_sec    st_atime
#define st_atime_nsec   st_atimensec
#define st_mtime_sec    st_mtime
#define st_mtime_nsec   st_mtimensec
#endif
#endif /* __APPLE__ */
/*
 * ftree.c
 */
int ftree_start(void);
int ftree_add(char *, int);
void ftree_sel(ARCHD *);
void ftree_notsel(void);
void ftree_chk(void);
int next_file(ARCHD *);

/*
 * gen_subs.c
 */
void ls_list(ARCHD *, time_t, FILE *);
void ls_tty(ARCHD *);
#ifdef __APPLE__
void safe_print(const char *, FILE *);
#endif /* __APPLE__ */
int l_strncpy(char *, const char *, int);
u_long asc_ul(char *, int, int);
int ul_asc(u_long, char *, int, int);
u_quad_t asc_uqd(char *, int, int);
int uqd_asc(u_quad_t, char *, int, int);

/*
 * getoldopt.c
 */
int getoldopt(int, char **, const char *);

/*
 * options.c
 */
extern FSUB fsub[];
extern int ford[];
void options(int, char **);
OPLIST * opt_next(void);
int opt_add(const char *);
int bad_opt(void);
#ifdef __APPLE__
int pax_format_opt_add(char *);
int pax_opt(void);
#endif /* __APPLE__ */
extern char *chdname;

/*
 * pat_rep.c
 */
int rep_add(char *);
int pat_add(char *, char *);
void pat_chk(void);
int pat_sel(ARCHD *);
int pat_match(ARCHD *);
int mod_name(ARCHD *);
int set_dest(ARCHD *, char *, int);

/*
 * pax.c
 */
extern int act;
extern FSUB *frmt;
extern int cflag;
extern int cwdfd;
extern int dflag;
extern int iflag;
extern int kflag;
extern int lflag;
extern int nflag;
extern int tflag;
extern int uflag;
extern int vflag;
extern int Dflag;
extern int Hflag;
extern int Lflag;
extern int Oflag;
extern int Xflag;
extern int Yflag;
extern int Zflag;
#ifdef __APPLE__
extern int zeroflag;
#endif /* __APPLE__ */
extern int vfpart;
extern int patime;
extern int pmtime;
extern int nodirs;
extern int pmode;
extern int pids;
extern int rmleadslash;
#ifdef __APPLE__
extern int secure;
#endif /* __APPLE__ */
extern int exit_val;
extern int docrc;
extern char *dirptr;
extern const char *argv0;
extern sigset_t s_mask;
extern FILE *listf;
extern char *tempfile;
extern char *tempbase;
#ifdef __APPLE__
extern int havechd;
#endif /* __APPLE__ */

void sig_cleanup(int);

/*
 * sel_subs.c
 */
int sel_chk(ARCHD *);
int grp_add(char *);
int usr_add(char *);
int trng_add(char *);

/*
 * tables.c
 */
int lnk_start(void);
int chk_lnk(ARCHD *);
void purg_lnk(ARCHD *);
void lnk_end(void);
int ftime_start(void);
int chk_ftime(ARCHD *);
int name_start(void);
int add_name(char *, int, char *);
void sub_name(char *, int *, size_t);
int dev_start(void);
int add_dev(ARCHD *);
int map_dev(ARCHD *, u_long, u_long);
int atdir_start(void);
void atdir_end(void);
#ifdef __APPLE__
void add_atdir(char *, dev_t, ino_t, time_t, time_t, time_t, time_t);
int get_atdir(dev_t, ino_t, time_t *, time_t *, time_t *, time_t *);
#else
void add_atdir(char *, dev_t, ino_t, time_t, time_t);
int get_atdir(dev_t, ino_t, time_t *, time_t *);
#endif /* __APPLE__ */
int dir_start(void);
#ifdef __APPLE__
void add_dir(char *, size_t, struct stat *, int);
#else
void add_dir(char *, int, struct stat *, int);
#endif /* __APPLE__ */
void proc_dir(void);
u_int st_hash(char *, int, int);

/*
 * tar.c
 */
#ifdef __APPLE__
extern char *gnu_hack_string;
#endif /* __APPLE__ b*/
int tar_endwr(void);
off_t tar_endrd(void);
int tar_trail(char *, int, int *);
int tar_id(char *, int);
int tar_opt(void);
int tar_rd(ARCHD *, char *);
int tar_wr(ARCHD *);
int ustar_strd(void);
int ustar_stwr(void);
int ustar_id(char *, int);
int ustar_rd(ARCHD *, char *);
int ustar_wr(ARCHD *);


#ifdef __APPLE__
/*
 * pax_format.c
 */
extern char *header_name_g;
extern int pax_read_or_list_mode;
#define PAX_INVALID_ACTION_BYPASS	1
#define PAX_INVALID_ACTION_RENAME	2
#define PAX_INVALID_ACTION_UTF8		3
#define PAX_INVALID_ACTION_WRITE	4
extern int want_linkdata;
extern int pax_invalid_action;
extern char *	pax_list_opt_format;
extern char *	pax_invalid_action_write_path;
extern char *	pax_invalid_action_write_cwd;
void pax_format_list_output(ARCHD *, time_t, FILE *, int);
void cleanup_pax_invalid_action(void);
void record_pax_invalid_action_results(ARCHD *, char *);
int perform_pax_invalid_action(ARCHD *, int);
void adjust_copy_for_pax_options(ARCHD *);
/*
int pax_strd(void);
int pax_stwr(void);
*/
int pax_id(char *, int);
int pax_rd(ARCHD *, char *);
int pax_wr(ARCHD *);
#endif /* __APPLE__ */


/*
 * tty_subs.c
 */
int tty_init(void);
void tty_prnt(const char *, ...) __printflike(1, 2);
int tty_read(char *, int);
void paxwarn(int, const char *, ...) __printflike(2, 3);
void syswarn(int, int, const char *, ...) __printflike(3, 4);

#endif /* _PAX_EXTERN_H_ */
