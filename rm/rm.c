/*-
 * Copyright (c) 1990, 1993, 1994
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
__used static const char copyright[] =
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rm.c	8.5 (Berkeley) 4/18/94";
#else
__used static const char rcsid[] =
  "$FreeBSD: src/bin/rm/rm.c,v 1.33 2001/06/13 15:01:25 ru Exp $";
#endif
#endif /* not lint */

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <locale.h>

#ifdef __APPLE__
#include <removefile.h>
#include <pwd.h>
#include <grp.h>
#include "get_compat.h"
#include "rm_trash.h"

#ifndef AT_REMOVEDIR_DATALESS
#define AT_REMOVEDIR_DATALESS   0x0100  /* Remove a dataless directory without materializing first */
#endif
#else
#define COMPAT_MODE(func, mode) 1
#endif

int dflag, eval, fflag, iflag, Pflag, vflag, Wflag, tflag, stdin_ok;
uid_t uid;

int	check __P((char *, char *, struct stat *));
int checkdir __P((char *));
int		yes_or_no __P((void));
void	checkdot __P((char **));
void	rm_file __P((char **));
void	rm_overwrite __P((char *, struct stat *));
void	rm_tree __P((char **));
void	usage __P((void));

/*
 * rm --
 *	This rm is different from historic rm's, but is expected to match
 *	POSIX 1003.2 behavior.  The most visible difference is that -f
 *	has two specific effects now, ignore non-existent files and force
 * 	file removal.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, rflag;
	char *p;

	if (argc < 1)
		usage();

	/*
	 * Test for the special case where the utility is called as
	 * "unlink", for which the functionality provided is greatly
	 * simplified.
	 */
	if ((p = rindex(argv[0], '/')) == NULL)
		p = argv[0];
	else
		++p;
	uid = geteuid();
	if (strcmp(p, "unlink") == 0) {
		if (argc == 2) {
			rm_file(&argv[1]);
			exit(eval);
		} else 
			usage();
	}

	Pflag = rflag = 0;
	while ((ch = getopt(argc, argv, "dfiPRrvWt")) != -1)
		switch(ch) {
		case 'd':
			dflag = 1;
			break;
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'i':
			fflag = 0;
			iflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'R':
		case 'r':			/* Compatibility. */
			rflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'W':
			Wflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		if (fflag)
			return 0;
		usage();
	}

	tflag |= !!getenv("RM_USE_TRASH");

	checkdot(argv);

	if (*argv) {
		stdin_ok = isatty(STDIN_FILENO);

		if (rflag) {
			if (tflag)
				eval = rm_tree_trash(argv);
			else
				rm_tree(argv);
		}
		else {
			rm_file(argv);
		}
	}

	exit (eval);
}

void
rm_tree(argv)
	char **argv;
{
	errno_t dir_errno = -1;
	FTS *fts;
	FTSENT *p;
	int needstat;
	int flags;
	int rval;
	int wantConformance = COMPAT_MODE("bin/rm", "unix2003");
	/*
	 * Remove a file hierarchy.  If forcing removal (-f), or interactive
	 * (-i) or can't ask anyway (stdin_ok), don't stat the file.
	 */
	needstat = !uid || (!fflag && !iflag && stdin_ok);

	/*
	 * If the -i option is specified, the user can skip on the pre-order
	 * visit.  The fts_number field flags skipped directories.
	 */
#define	SKIPPED	1

	flags = FTS_PHYSICAL;
	if (!needstat)
		flags |= FTS_NOSTAT;
	if (Wflag)
		flags |= FTS_WHITEOUT;
	if (!(fts = fts_open(argv, flags, NULL))) {
		if (fflag && errno == ENOENT)
			return;
		err(1, NULL);
	}
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			if (!fflag || p->fts_errno != ENOENT) {
				warnx("%s: %s",
				    p->fts_path, strerror(p->fts_errno));
				eval = 1;
			}
			continue;
		case FTS_ERR:
			errx(1, "%s: %s", p->fts_path, strerror(p->fts_errno));
		case FTS_NS:
			/*
			 * FTS_NS: assume that if can't stat the file, it
			 * can't be unlinked.
			 */
			if (!needstat)
				break;
			if (!fflag || p->fts_errno != ENOENT) {
				warnx("%s: %s",
				    p->fts_path, strerror(p->fts_errno));
				eval = 1;
			}
			continue;
		case FTS_D:
			/* Pre-order: give user chance to skip. */
			/* In conformance mode the user is prompted to skip processing the contents.
			 * Then the option to delete the dir is presented post-order */
			if (!fflag && 
					( (wantConformance && !checkdir(p->fts_path)) ||
					  (!wantConformance && !check(p->fts_path, p->fts_accpath, p->fts_statp))
					)
			   ){
				(void)fts_set(fts, p, FTS_SKIP);
				p->fts_number = SKIPPED;
			}
			else if (!uid &&
				 (p->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
				 !(p->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
				 chflags(p->fts_accpath,
					 p->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE)) < 0)
				goto err;
			continue;
		case FTS_DP:
			/* Post-order: see if user skipped. */
			if(p->fts_number == SKIPPED)/*in legacy mode, the user was prompted pre-order */
				continue;
			else if(wantConformance)
			{
				/* delete directory if force is on, or if user answers Y to prompt */
				if(fflag || check(p->fts_path, p->fts_accpath, p->fts_statp)) 
					break;
				else
					continue;
			}
			break;
		default:
			if (!fflag &&
			    !check(p->fts_path, p->fts_accpath, p->fts_statp))
				continue;
		}

		rval = 0;
		if (!uid &&
		    (p->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
		    !(p->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)))
			rval = chflags(p->fts_accpath,
				       p->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE));
		if (rval == 0) {
			/*
			 * If we can't read or search the directory, may still be
			 * able to remove it.  Don't print out the un{read,search}able
			 * message unless the remove fails.
			 */
			switch (p->fts_info) {
			case FTS_DP:
			case FTS_DNR:
#if __APPLE__
				rval = unlinkat(AT_FDCWD, p->fts_accpath, AT_REMOVEDIR_DATALESS);
				if (rval == -1 && errno == EINVAL) {
					/*
					 * Kernel rejected AT_REMOVEDIR_DATALESS?
					 * I guess we fall back on the painful
					 * route (but it's better than failing).
					 */
					rval = rmdir(p->fts_accpath);
				}
#else
				rval = rmdir(p->fts_accpath);
#endif
				if (rval == 0 || (fflag && errno == ENOENT)) {
					if (rval == 0 && vflag)
						(void)printf("%s\n",
						    p->fts_path);
					continue;
				}

				if (rval == -1) {
					if (p->fts_info == FTS_DP && errno == ENOTEMPTY && (dir_errno == EACCES || dir_errno == -1)) {
						/*
						 * If we reach this point, it means that we either failed to
						 * delete an entry because of a permission error,
						 * Or because we couldn't even list the dir entities.
						 * This case should be EACCES.
						 */
						errno = EACCES;
					}

					if ((dir_errno == -1) || (dir_errno == EACCES))
						dir_errno = errno;
				}

				break;

			case FTS_W:
				rval = undelete(p->fts_accpath);
				if (rval == 0 && (fflag && errno == ENOENT)) {
					if (vflag)
						(void)printf("%s\n",
						    p->fts_path);
					continue;
				}
				break;

			default:
#ifdef __APPLE__
				if (Pflag) {
					if (removefile(p->fts_accpath, NULL, REMOVEFILE_SECURE_7_PASS)) /* overwrites and unlinks */
						eval = rval = 1;
				} else
					rval = unlink(p->fts_accpath);
#else  /* !__APPLE_ */
				if (Pflag)
					rm_overwrite(p->fts_accpath, NULL);
				rval = unlink(p->fts_accpath);
#endif	/* __APPLE__ */
				if (rval == 0 || (fflag && errno == ENOENT)) {
					if (rval == 0 && vflag)
						(void)printf("%s\n",
						    p->fts_path);
					continue;
				}
				if (rval == -1) {
					if ((dir_errno == -1) || (dir_errno == EACCES))
						dir_errno = errno;
				}
			}
		}
err:
		warn("%s", p->fts_path);
		eval = 1;
	}
	if (errno)
		err(1, "fts_read");
	fts_close(fts);
}

void
rm_file(argv)
	char **argv;
{
	struct stat sb;
	int rval;
	char *f;

	/*
	 * Remove a file.  POSIX 1003.2 states that, by default, attempting
	 * to remove a directory is an error, so must always stat the file.
	 */
	while ((f = *argv++) != NULL) {
		/* Assume if can't stat the file, can't unlink it. */
		if (lstat(f, &sb)) {
			if (Wflag) {
				sb.st_mode = S_IFWHT|S_IWUSR|S_IRUSR;
			} else {
				if (!fflag || errno != ENOENT) {
					warn("%s", f);
					eval = 1;
				}
				continue;
			}
		} else if (Wflag) {
			warnx("%s: %s", f, strerror(EEXIST));
			eval = 1;
			continue;
		}

		if (S_ISDIR(sb.st_mode) && !dflag) {
			warnx("%s: is a directory", f);
			eval = 1;
			continue;
		}
		if (!fflag && !S_ISWHT(sb.st_mode) && !check(f, f, &sb))
			continue;
		rval = 0;
		if (!uid &&
		    (sb.st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
		    !(sb.st_flags & (SF_APPEND|SF_IMMUTABLE)))
			rval = chflags(f, sb.st_flags & ~(UF_APPEND|UF_IMMUTABLE));
		if (rval == 0) {
			if (S_ISWHT(sb.st_mode))
				rval = undelete(f);
			else if (S_ISDIR(sb.st_mode))
				rval = rmdir(f);
			else {
#ifdef __APPLE__
				if (tflag) {
					eval = rval = removefileToTrash(f);
				} else {
					if (Pflag) {
						if (removefile(f, NULL, REMOVEFILE_SECURE_7_PASS)) /* overwrites and unlinks */
							eval = rval = 1;
					} else
						rval = unlink(f);
				}
#else  /* !__APPLE__ */
				if (Pflag)
					rm_overwrite(f, &sb);
				rval = unlink(f);
#endif	/* __APPLE__ */
			}
		}
		if (rval && (!fflag || errno != ENOENT)) {
			warn("%s", f);
			eval = 1;
		}
		if (vflag && rval == 0)
			(void)printf("%s\n", f);
	}
}

/*
 * rm_overwrite --
 *	Overwrite the file 3 times with varying bit patterns.
 *
 * XXX
 * This is a cheap way to *really* delete files.  Note that only regular
 * files are deleted, directories (and therefore names) will remain.
 * Also, this assumes a fixed-block file system (like FFS, or a V7 or a
 * System V file system).  In a logging file system, you'll have to have
 * kernel support.
 */
void
rm_overwrite(file, sbp)
	char *file;
	struct stat *sbp;
{
	struct stat sb;
	struct statfs fsb;
	off_t len;
	int bsize, fd, wlen;
	char *buf = NULL;

	if (sbp == NULL) {
		if (lstat(file, &sb))
			goto err;
		sbp = &sb;
	}
	if (!S_ISREG(sbp->st_mode))
		return;
	if ((fd = open(file, O_WRONLY, 0)) == -1)
		goto err;
	if (fstatfs(fd, &fsb) == -1)
		goto err;
	bsize = MAX(fsb.f_iosize, 1024);
	if ((buf = malloc(bsize)) == NULL)
		err(1, "malloc");

#define	PASS(byte) {							\
	memset(buf, byte, bsize);					\
	for (len = sbp->st_size; len > 0; len -= wlen) {		\
		wlen = len < bsize ? (int)len : bsize;			\
		if (write(fd, buf, wlen) != wlen)			\
			goto err;					\
	}								\
}
	PASS(0xff);
	if (fsync(fd) || lseek(fd, (off_t)0, SEEK_SET))
		goto err;
	PASS(0x00);
	if (fsync(fd) || lseek(fd, (off_t)0, SEEK_SET))
		goto err;
	PASS(0xff);
	if (!fsync(fd) && !close(fd)) {
		free(buf);
		return;
	}

err:	eval = 1;
	if (buf)
		free(buf);
	warn("%s", file);
}

int 
yes_or_no()
{
	int ch, first;
	char resp[] = {'\0', '\0'};

	(void)fflush(stderr);

	/* Load user specified locale */
	setlocale(LC_MESSAGES, "");

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();

	/* only care about the first character */
	resp[0] = first;

	return (rpmatch(resp) == 1);
}

int
checkdir(path)
	char *path;
{
	if(!iflag)
		return 1;	//if not interactive, process directory's contents
	(void)fprintf(stderr, "examine files in directory %s? ", path);
	return yes_or_no();
}

int
check(path, name, sp)
	char *path, *name;
	struct stat *sp;
{
	char modep[15], *flagsp;

	/* Check -i first. */
	if (iflag)
		(void)fprintf(stderr, "remove %s? ", path);
	else {
		/*
		 * If it's not a symbolic link and it's unwritable and we're
		 * talking to a terminal, ask.  Symbolic links are excluded
		 * because their permissions are meaningless.  Check stdin_ok
		 * first because we may not have stat'ed the file.
		 */
		if (!stdin_ok || S_ISLNK(sp->st_mode) ||
		    (!access(name, W_OK) &&
		    !(sp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
		    (!(sp->st_flags & (UF_APPEND|UF_IMMUTABLE)) || !uid)))
			return (1);
		strmode(sp->st_mode, modep);
		if ((flagsp = fflagstostr(sp->st_flags)) == NULL)
			err(1, NULL);
		(void)fprintf(stderr, "override %s%s%s/%s %s%sfor %s? ",
		    modep + 1, modep[9] == ' ' ? "" : " ",
		    user_from_uid(sp->st_uid, 0),
		    group_from_gid(sp->st_gid, 0),
		    *flagsp ? flagsp : "", *flagsp ? " " : "", 
		    path);
		free(flagsp);
	}
	return yes_or_no();
}


#define ISDOT(a)	((a)[0] == '.' && (!(a)[1] || ((a)[1] == '.' && !(a)[2])))
void
checkdot(argv)
	char **argv;
{
	char *p, **save, **t;
	int complained;

	complained = 0;
	for (t = argv; *t;) {
		size_t len = strlen(*t);
		char truncated[len];

		if ((p = strrchr(*t, '/')) != NULL) {
			if (p[1] == '\0') { // one or more trailing / -- treat as if not present
				for (; (p > *t) && (p[-1] == '/');) {
					len--;
					p--;
				}
				strlcpy(truncated, *t, len);
				p = strrchr(truncated, '/');
				if (p) {
					++p;
				} else {
					p = truncated;
				}
			} else {
				++p;
			}
		} else {
			p = *t;
		}
		if (ISDOT(p)) {
			if (!complained++)
				warnx("\".\" and \"..\" may not be removed");
			eval = 1;
			for (save = t; (t[0] = t[1]) != NULL; ++t)
				continue;
			t = save;
		} else
			++t;
	}
}

void
usage()
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: rm [-f | -i] [-dPRrvWt] file ...",
	    "       unlink file");
	exit(EX_USAGE);
}
