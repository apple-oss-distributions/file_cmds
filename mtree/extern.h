/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.sbin/mtree/extern.h,v 1.13 2004/01/11 19:38:48 phk Exp $
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include "mtree.h"

extern uint32_t crc_total;

#ifdef _FTS_H_
int	 compare(char *, NODE *, FTSENT *);
#endif
int	 crc(int, uint32_t *, off_t *);
void	 cwalk(void);
char	*flags_to_string(u_long);
char	*escape_path(char *string);
struct timespec	ptime(char *path, int *supported);

const char	*inotype(u_int);
u_int	 parsekey(char *, int *);
char	*rlink(char *);
NODE	*mtree_readspec(FILE *fi);
int	mtree_verifyspec(FILE *fi);
int	mtree_specspec(FILE *fi, FILE *fj);

int	 check_excludes(const char *, const char *);
void	 init_excludes(void);
void	 read_excludes_file(const char *);
const char * ftype(u_int type);

extern int ftsoptions;
extern int xattr_options;
extern u_int keys;
extern int lineno;
extern int dflag, eflag, iflag, nflag, qflag, rflag, sflag, uflag, wflag, mflag, tflag, xflag;
extern int insert_mod, insert_birth, insert_access, insert_change, insert_parent;
extern struct timespec ts;
#ifdef MAXPATHLEN
extern char fullpath[MAXPATHLEN];
#endif

#endif /* _EXTERN_H_ */
