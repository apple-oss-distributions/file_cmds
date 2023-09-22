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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tar.c	8.2 (Berkeley) 4/18/94";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#endif /* __APPLE__ */
#include "pax.h"
#include "extern.h"
#include "tar.h"

/*
 * Routines for reading, writing and header identify of various versions of tar
 */

#ifdef __APPLE__
static size_t expandname(char *, size_t, char **, const char *, size_t);
#endif /* __APPLE__ */
static u_long tar_chksm(char *, int);
static char *name_split(char *, int);
static int ul_oct(u_long, char *, int, int);
static int uqd_oct(u_quad_t, char *, int, int);

#ifdef __APPLE__
static uid_t uid_nobody;
static uid_t uid_warn;
static gid_t gid_nobody;
static gid_t gid_warn;
#endif /* __APPLE__ */

/*
 * Routines common to all versions of tar
 */

static int tar_nodir;			/* do not write dirs under old tar */
#ifdef __APPLE__
char *gnu_name_string;			/* GNU ././@LongLink hackery name */
char *gnu_link_string;			/* GNU ././@LongLink hackery link */
#endif /* __APPLE__ */

/*
 * tar_endwr()
 *	add the tar trailer of two null blocks
 * Return:
 *	0 if ok, -1 otherwise (what wr_skip returns)
 */

int
tar_endwr(void)
{
	return(wr_skip((off_t)(NULLCNT*BLKMULT)));
}

/*
 * tar_endrd()
 *	no cleanup needed here, just return size of trailer (for append)
 * Return:
 *	size of trailer (2 * BLKMULT)
 */

off_t
tar_endrd(void)
{
	return((off_t)(NULLCNT*BLKMULT));
}

/*
 * tar_trail()
 *	Called to determine if a header block is a valid trailer. We are passed
 *	the block, the in_sync flag (which tells us we are in resync mode;
 *	looking for a valid header), and cnt (which starts at zero) which is
 *	used to count the number of empty blocks we have seen so far.
 * Return:
 *	0 if a valid trailer, -1 if not a valid trailer, or 1 if the block
 *	could never contain a header.
 */

int
tar_trail(char *buf, int in_resync, int *cnt)
{
	int i;

	/*
	 * look for all zero, trailer is two consecutive blocks of zero
	 */
	for (i = 0; i < BLKMULT; ++i) {
		if (buf[i] != '\0')
			break;
	}

	/*
	 * if not all zero it is not a trailer, but MIGHT be a header.
	 */
	if (i != BLKMULT)
		return(-1);

	/*
	 * When given a zero block, we must be careful!
	 * If we are not in resync mode, check for the trailer. Have to watch
	 * out that we do not mis-identify file data as the trailer, so we do
	 * NOT try to id a trailer during resync mode. During resync mode we
	 * might as well throw this block out since a valid header can NEVER be
	 * a block of all 0 (we must have a valid file name).
	 */
	if (!in_resync && (++*cnt >= NULLCNT))
		return(0);
	return(1);
}

/*
 * ul_oct()
 *	convert an unsigned long to an octal string. many oddball field
 *	termination characters are used by the various versions of tar in the
 *	different fields. term selects which kind to use. str is '0' padded
 *	at the front to len. we are unable to use only one format as many old
 *	tar readers are very cranky about this.
 * Return:
 *	0 if the number fit into the string, -1 otherwise
 */

static int
ul_oct(u_long val, char *str, int len, int term)
{
	char *pt;

	/*
	 * term selects the appropriate character(s) for the end of the string
	 */
	pt = str + len - 1;
	switch(term) {
	case 3:
		*pt-- = '\0';
		break;
	case 2:
		*pt-- = ' ';
		*pt-- = '\0';
		break;
	case 1:
		*pt-- = ' ';
		break;
	case 0:
	default:
		*pt-- = '\0';
		*pt-- = ' ';
		break;
	}

	/*
	 * convert and blank pad if there is space
	 */
	while (pt >= str) {
		*pt-- = '0' + (char)(val & 0x7);
		if ((val = val >> 3) == (u_long)0)
			break;
	}

	while (pt >= str)
		*pt-- = '0';
	if (val != (u_long)0)
		return(-1);
	return(0);
}

/*
 * uqd_oct()
 *	convert an u_quad_t to an octal string. one of many oddball field
 *	termination characters are used by the various versions of tar in the
 *	different fields. term selects which kind to use. str is '0' padded
 *	at the front to len. we are unable to use only one format as many old
 *	tar readers are very cranky about this.
 * Return:
 *	0 if the number fit into the string, -1 otherwise
 */

static int
uqd_oct(u_quad_t val, char *str, int len, int term)
{
	char *pt;

	/*
	 * term selects the appropriate character(s) for the end of the string
	 */
	pt = str + len - 1;
	switch(term) {
	case 3:
		*pt-- = '\0';
		break;
	case 2:
		*pt-- = ' ';
		*pt-- = '\0';
		break;
	case 1:
		*pt-- = ' ';
		break;
	case 0:
	default:
		*pt-- = '\0';
		*pt-- = ' ';
		break;
	}

	/*
	 * convert and blank pad if there is space
	 */
	while (pt >= str) {
		*pt-- = '0' + (char)(val & 0x7);
		if ((val = val >> 3) == 0)
			break;
	}

	while (pt >= str)
		*pt-- = '0';
	if (val != (u_quad_t)0)
		return(-1);
	return(0);
}

/*
 * tar_chksm()
 *	calculate the checksum for a tar block counting the checksum field as
 *	all blanks (BLNKSUM is that value pre-calculated, the sum of 8 blanks).
 *	NOTE: we use len to short circuit summing 0's on write since we ALWAYS
 *	pad headers with 0.
 * Return:
 *	unsigned long checksum
 */

static u_long
tar_chksm(char *blk, int len)
{
	char *stop;
	char *pt;
	u_long chksm = BLNKSUM;	/* initial value is checksum field sum */

	/*
	 * add the part of the block before the checksum field
	 */
	pt = blk;
	stop = blk + CHK_OFFSET;
	while (pt < stop)
		chksm += (u_long)(*pt++ & 0xff);
	/*
	 * move past the checksum field and keep going, spec counts the
	 * checksum field as the sum of 8 blanks (which is pre-computed as
	 * BLNKSUM).
	 * ASSUMED: len is greater than CHK_OFFSET. (len is where our 0 padding
	 * starts, no point in summing zero's)
	 */
	pt += CHK_LEN;
	stop = blk + len;
	while (pt < stop)
		chksm += (u_long)(*pt++ & 0xff);
	return(chksm);
}

/*
 * Routines for old BSD style tar (also made portable to sysV tar)
 */

/*
 * tar_id()
 *	determine if a block given to us is a valid tar header (and not a USTAR
 *	header). We have to be on the lookout for those pesky blocks of all
 *	zero's.
 * Return:
 *	0 if a tar header, -1 otherwise
 */

int
tar_id(char *blk, int size)
{
	HD_TAR *hd;
	HD_USTAR *uhd;

	if (size < BLKMULT)
		return(-1);
	hd = (HD_TAR *)blk;
	uhd = (HD_USTAR *)blk;

	/*
	 * check for block of zero's first, a simple and fast test, then make
	 * sure this is not a ustar header by looking for the ustar magic
	 * cookie. We should use TMAGLEN, but some USTAR archive programs are
	 * wrong and create archives missing the \0. Last we check the
	 * checksum. If this is ok we have to assume it is a valid header.
	 */
	if (hd->name[0] == '\0')
		return(-1);
	if (strncmp(uhd->magic, TMAGIC, TMAGLEN - 1) == 0)
		return(-1);
	if (asc_ul(hd->chksum,sizeof(hd->chksum),OCT) != tar_chksm(blk,BLKMULT))
		return(-1);
#ifdef __APPLE__
	Oflag = 1;
#endif /* __APPLE__ */
	return(0);
}

/*
 * tar_opt()
 *	handle tar format specific -o options
 * Return:
 *	0 if ok -1 otherwise
 */

int
tar_opt(void)
{
	OPLIST *opt;

	while ((opt = opt_next()) != NULL) {
		if (strcmp(opt->name, TAR_OPTION) ||
		    strcmp(opt->value, TAR_NODIR)) {
			paxwarn(1, "Unknown tar format -o option/value pair %s=%s",
			    opt->name, opt->value);
			paxwarn(1,"%s=%s is the only supported tar format option",
			    TAR_OPTION, TAR_NODIR);
			return(-1);
		}

		/*
		 * we only support one option, and only when writing
		 */
		if ((act != APPND) && (act != ARCHIVE)) {
			paxwarn(1, "%s=%s is only supported when writing.",
			    opt->name, opt->value);
			return(-1);
		}
		tar_nodir = 1;
	}
	return(0);
}


/*
 * tar_rd()
 *	extract the values out of block already determined to be a tar header.
 *	store the values in the ARCHD parameter.
 * Return:
 *	0
 */

int
tar_rd(ARCHD *arcn, char *buf)
{
	HD_TAR *hd;
	char *pt;

	/*
	 * we only get proper sized buffers passed to us
	 */
	if (tar_id(buf, BLKMULT) < 0)
		return(-1);
#ifdef __APPLE__
	memset(arcn, 0, sizeof(*arcn));
#endif /* __APPLE__ */
	arcn->org_name = arcn->name;
	arcn->sb.st_nlink = 1;
#ifndef __APPLE__
	arcn->pat = NULL;
#endif /* __APPLE__ */

	/*
	 * copy out the name and values in the stat buffer
	 */
	hd = (HD_TAR *)buf;
#ifdef __APPLE__
	if (hd->linkflag != LONGLINKTYPE && hd->linkflag != LONGNAMETYPE) {
		arcn->nlen = expandname(arcn->name, sizeof(arcn->name),
		    &gnu_name_string, hd->name, sizeof(hd->name));
		arcn->ln_nlen = expandname(arcn->ln_name, sizeof(arcn->ln_name),
		    &gnu_link_string, hd->linkname, sizeof(hd->linkname));
	}
#else
	/*
	 * old tar format specifies the name always be null-terminated,
	 * but let's be robust to broken archives.
	 * the same applies to handling links below.
	 */
	arcn->nlen = l_strncpy(arcn->name, hd->name,
	    MIN(sizeof(hd->name), sizeof(arcn->name)) - 1);
	arcn->name[arcn->nlen] = '\0';
#endif /* __APPLE__ */
	arcn->sb.st_mode = (mode_t)(asc_ul(hd->mode,sizeof(hd->mode),OCT) &
	    0xfff);
	arcn->sb.st_uid = (uid_t)asc_ul(hd->uid, sizeof(hd->uid), OCT);
	arcn->sb.st_gid = (gid_t)asc_ul(hd->gid, sizeof(hd->gid), OCT);
	arcn->sb.st_size = (off_t)asc_uqd(hd->size, sizeof(hd->size), OCT);
	arcn->sb.st_mtime = (time_t)asc_uqd(hd->mtime, sizeof(hd->mtime), OCT);
	arcn->sb.st_ctime = arcn->sb.st_atime = arcn->sb.st_mtime;

	/*
	 * have to look at the last character, it may be a '/' and that is used
	 * to encode this as a directory
	 */
	pt = &(arcn->name[arcn->nlen - 1]);
	arcn->pad = 0;
	arcn->skip = 0;
	switch(hd->linkflag) {
	case SYMTYPE:
		/*
		 * symbolic link, need to get the link name and set the type in
		 * the st_mode so -v printing will look correct.
		 */
		arcn->type = PAX_SLK;
#ifndef __APPLE__
		arcn->ln_nlen = l_strncpy(arcn->ln_name, hd->linkname,
		    MIN(sizeof(hd->linkname), sizeof(arcn->ln_name)) - 1);
		arcn->ln_name[arcn->ln_nlen] = '\0';
#endif /* __APPLE__ */
		arcn->sb.st_mode |= S_IFLNK;
		break;
	case LNKTYPE:
		/*
		 * hard link, need to get the link name, set the type in the
		 * st_mode and st_nlink so -v printing will look better.
		 */
		arcn->type = PAX_HLK;
		arcn->sb.st_nlink = 2;
		arcn->ln_nlen = l_strncpy(arcn->ln_name, hd->linkname,
		    MIN(sizeof(hd->linkname), sizeof(arcn->ln_name)) - 1);
		arcn->ln_name[arcn->ln_nlen] = '\0';

		/*
		 * no idea of what type this thing really points at, but
		 * we set something for printing only.
		 */
		arcn->sb.st_mode |= S_IFREG;
		break;
#ifdef __APPLE__
	case LONGLINKTYPE:
	case LONGNAMETYPE:
		/*
		 * GNU long link/file; we tag these here and let the
		 * pax internals deal with it -- too ugly otherwise.
		 */
		arcn->type =
		    hd->linkflag == LONGLINKTYPE ? PAX_GLL : PAX_GLF;
		arcn->pad = TAR_PAD(arcn->sb.st_size);
		arcn->skip = arcn->sb.st_size;
		break;
#endif /* __APPLE__ */
	case DIRTYPE:
		/*
		 * It is a directory, set the mode for -v printing
		 */
		arcn->type = PAX_DIR;
		arcn->sb.st_mode |= S_IFDIR;
		arcn->sb.st_nlink = 2;
		arcn->ln_name[0] = '\0';
		arcn->ln_nlen = 0;
		break;
	case AREGTYPE:
	case REGTYPE:
	default:
		/*
		 * If we have a trailing / this is a directory and NOT a file.
		 */
		arcn->ln_name[0] = '\0';
		arcn->ln_nlen = 0;
		if (*pt == '/') {
			/*
			 * it is a directory, set the mode for -v printing
			 */
			arcn->type = PAX_DIR;
			arcn->sb.st_mode |= S_IFDIR;
			arcn->sb.st_nlink = 2;
		} else {
			/*
			 * have a file that will be followed by data. Set the
			 * skip value to the size field and calculate the size
			 * of the padding.
			 */
			arcn->type = PAX_REG;
			arcn->sb.st_mode |= S_IFREG;
			arcn->pad = TAR_PAD(arcn->sb.st_size);
			arcn->skip = arcn->sb.st_size;
		}
		break;
	}

	/*
	 * strip off any trailing slash.
	 */
	if (*pt == '/') {
		*pt = '\0';
		--arcn->nlen;
	}
	return(0);
}

/*
 * tar_wr()
 *	write a tar header for the file specified in the ARCHD to the archive.
 *	Have to check for file types that cannot be stored and file names that
 *	are too long. Be careful of the term (last arg) to ul_oct, each field
 *	of tar has it own spec for the termination character(s).
 *	ASSUMED: space after header in header block is zero filled
 * Return:
 *	0 if file has data to be written after the header, 1 if file has NO
 *	data to write after the header, -1 if archive write failed
 */

int
tar_wr(ARCHD *arcn)
{
	HD_TAR *hd;
	int len;
	HD_TAR hdblk;

	/*
	 * check for those file system types which tar cannot store
	 */
	switch(arcn->type) {
	case PAX_DIR:
		/*
		 * user asked that dirs not be written to the archive
		 */
		if (tar_nodir)
			return(1);
		break;
	case PAX_CHR:
		paxwarn(1, "Tar cannot archive a character device %s",
		    arcn->org_name);
		return(1);
	case PAX_BLK:
		paxwarn(1, "Tar cannot archive a block device %s", arcn->org_name);
		return(1);
	case PAX_SCK:
		paxwarn(1, "Tar cannot archive a socket %s", arcn->org_name);
		return(1);
	case PAX_FIF:
		paxwarn(1, "Tar cannot archive a fifo %s", arcn->org_name);
		return(1);
	case PAX_SLK:
	case PAX_HLK:
	case PAX_HRG:
		if (arcn->ln_nlen >= (int)sizeof(hd->linkname)) {
			paxwarn(1,"Link name too long for tar %s", arcn->ln_name);
			return(1);
		}
		break;
	case PAX_REG:
	case PAX_CTG:
	default:
		break;
	}

	/*
	 * check file name len, remember extra char for dirs (the / at the end)
	 */
	len = arcn->nlen;
	if (arcn->type == PAX_DIR)
		++len;
	if (len >= (int)sizeof(hd->name)) {
		paxwarn(1, "File name too long for tar %s", arcn->name);
		return(1);
	}

	/*
	 * Copy the data out of the ARCHD into the tar header based on the type
	 * of the file. Remember, many tar readers want all fields to be
	 * padded with zero so we zero the header first.  We then set the
	 * linkflag field (type), the linkname, the size, and set the padding
	 * (if any) to be added after the file data (0 for all other types,
	 * as they only have a header).
	 */
#ifdef __APPLE__
	memset(&hdblk, 0, sizeof(hdblk));
	hd = (HD_TAR *)&hdblk;
	strlcpy(hd->name,  arcn->name, sizeof(hd->name));
#else
	hd = &hdblk;
	l_strncpy(hd->name, arcn->name, sizeof(hd->name) - 1);
	hd->name[sizeof(hd->name) - 1] = '\0';
#endif /* __APPLE__ */
	arcn->pad = 0;

	if (arcn->type == PAX_DIR) {
		/*
		 * directories are the same as files, except have a filename
		 * that ends with a /, we add the slash here. No data follows,
		 * dirs, so no pad.
		 */
		hd->linkflag = AREGTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
#endif /* __APPLE__ */
		hd->name[len-1] = '/';
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 1))
			goto out;
	} else if (arcn->type == PAX_SLK) {
		/*
		 * no data follows this file, so no pad
		 */
		hd->linkflag = SYMTYPE;
#ifdef __APPLE__
		strlcpy(hd->linkname, arcn->ln_name, sizeof(hd->linkname));
#else
		l_strncpy(hd->linkname,arcn->ln_name, sizeof(hd->linkname) - 1);
		hd->linkname[sizeof(hd->linkname) - 1] = '\0';
#endif /* __APPLE__*/
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 1))
			goto out;
	} else if ((arcn->type == PAX_HLK) || (arcn->type == PAX_HRG)) {
		/*
		 * no data follows this file, so no pad
		 */
		hd->linkflag = LNKTYPE;
#ifdef __APPLE__
		strlcpy(hd->linkname, arcn->ln_name, sizeof(hd->linkname));
#else
		l_strncpy(hd->linkname,arcn->ln_name, sizeof(hd->linkname) - 1);
		hd->linkname[sizeof(hd->linkname) - 1] = '\0';
#endif /* __APPLE__*/
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 1))
			goto out;
	} else {
		/*
		 * data follows this file, so set the pad
		 */
		hd->linkflag = AREGTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
#endif /* __APPLE__ */
		if (uqd_oct((u_quad_t)arcn->sb.st_size, hd->size,
		    sizeof(hd->size), 1)) {
			paxwarn(1,"File is too large for tar %s", arcn->org_name);
			return(1);
		}
		arcn->pad = TAR_PAD(arcn->sb.st_size);
	}

	/*
	 * copy those fields that are independent of the type
	 */
	if (ul_oct((u_long)arcn->sb.st_mode, hd->mode, sizeof(hd->mode), 0) ||
	    ul_oct((u_long)arcn->sb.st_uid, hd->uid, sizeof(hd->uid), 0) ||
	    ul_oct((u_long)arcn->sb.st_gid, hd->gid, sizeof(hd->gid), 0) ||
	    ul_oct((u_long)arcn->sb.st_mtime, hd->mtime, sizeof(hd->mtime), 1))
		goto out;

	/*
	 * calculate and add the checksum, then write the header. A return of
	 * 0 tells the caller to now write the file data, 1 says no data needs
	 * to be written
	 */
	if (ul_oct(tar_chksm((char *)&hdblk, sizeof(HD_TAR)), hd->chksum,
	    sizeof(hd->chksum), 3))
		goto out;
	if (wr_rdbuf((char *)&hdblk, sizeof(HD_TAR)) < 0)
		return(-1);
	if (wr_skip((off_t)(BLKMULT - sizeof(HD_TAR))) < 0)
		return(-1);
	if ((arcn->type == PAX_CTG) || (arcn->type == PAX_REG))
		return(0);
	return(1);

    out:
	/*
	 * header field is out of range
	 */
	paxwarn(1, "Tar header field is too small for %s", arcn->org_name);
	return(1);
}

/*
 * Routines for POSIX ustar
 */

/*
 * ustar_strd()
 *	initialization for ustar read
 * Return:
 *	0 if ok, -1 otherwise
 */

int
ustar_strd(void)
{
	if ((usrtb_start() < 0) || (grptb_start() < 0))
		return(-1);
	return(0);
}

/*
 * ustar_stwr()
 *	initialization for ustar write
 * Return:
 *	0 if ok, -1 otherwise
 */

int
ustar_stwr(void)
{
	if ((uidtb_start() < 0) || (gidtb_start() < 0))
		return(-1);
	return(0);
}

/*
 * ustar_id()
 *	determine if a block given to us is a valid ustar header. We have to
 *	be on the lookout for those pesky blocks of all zero's
 * Return:
 *	0 if a ustar header, -1 otherwise
 */

int
ustar_id(char *blk, int size)
{
	HD_USTAR *hd;

	if (size < BLKMULT)
		return(-1);
	hd = (HD_USTAR *)blk;

	/*
	 * check for block of zero's first, a simple and fast test then check
	 * ustar magic cookie. We should use TMAGLEN, but some USTAR archive
	 * programs are fouled up and create archives missing the \0. Last we
	 * check the checksum. If ok we have to assume it is a valid header.
	 */
	if (hd->name[0] == '\0')
		return(-1);
	if (strncmp(hd->magic, TMAGIC, TMAGLEN - 1) != 0)
		return(-1);
	if (asc_ul(hd->chksum,sizeof(hd->chksum),OCT) != tar_chksm(blk,BLKMULT))
		return(-1);
	return(0);
}

/*
 * ustar_rd()
 *	extract the values out of block already determined to be a ustar header.
 *	store the values in the ARCHD parameter.
 * Return:
 *	0
 */

int
ustar_rd(ARCHD *arcn, char *buf)
{
	HD_USTAR *hd;
	char *dest;
	int cnt = 0;
	dev_t devmajor;
	dev_t devminor;

	/*
	 * we only get proper sized buffers
	 */
	if (ustar_id(buf, BLKMULT) < 0)
		return(-1);
	arcn->org_name = arcn->name;
	arcn->sb.st_nlink = 1;
	arcn->pat = NULL;
	arcn->nlen = 0;
	hd = (HD_USTAR *)buf;

	/*
	 * see if the filename is split into two parts. if, so joint the parts.
	 * we copy the prefix first and add a / between the prefix and name.
	 */
	dest = arcn->name;
	if (*(hd->prefix) != '\0') {
		cnt = l_strncpy(dest, hd->prefix,
		    MIN(sizeof(hd->prefix), sizeof(arcn->name) - 2));
		dest += cnt;
		*dest++ = '/';
		cnt++;
	}

#ifdef __APPLE__
	if (hd->typeflag != LONGLINKTYPE && hd->typeflag != LONGNAMETYPE) {
		arcn->nlen = cnt + expandname(dest, sizeof(arcn->name) - cnt,
		    &gnu_name_string, hd->name, sizeof(hd->name));
		arcn->ln_nlen = expandname(arcn->ln_name, sizeof(arcn->ln_name),
		    &gnu_link_string, hd->linkname, sizeof(hd->linkname));
	}
#endif /* __APPLE__ */

	/*
	 * ustar format specifies the name may be unterminated
	 * if it fills the entire field.  this also applies to
	 * the prefix and the linkname.
	 */
	arcn->nlen = cnt + l_strncpy(dest, hd->name,
	    MIN(sizeof(hd->name), sizeof(arcn->name) - cnt - 1));
	arcn->name[arcn->nlen] = '\0';

	/*
	 * follow the spec to the letter. we should only have mode bits, strip
	 * off all other crud we may be passed.
	 */
	arcn->sb.st_mode = (mode_t)(asc_ul(hd->mode, sizeof(hd->mode), OCT) &
	    0xfff);
	arcn->sb.st_size = (off_t)asc_uqd(hd->size, sizeof(hd->size), OCT);
	arcn->sb.st_mtime = (time_t)asc_uqd(hd->mtime, sizeof(hd->mtime), OCT);
	arcn->sb.st_ctime = arcn->sb.st_atime = arcn->sb.st_mtime;

	/*
	 * If we can find the ascii names for gname and uname in the password
	 * and group files we will use the uid's and gid they bind. Otherwise
	 * we use the uid and gid values stored in the header. (This is what
	 * the POSIX spec wants).
	 */
	hd->gname[sizeof(hd->gname) - 1] = '\0';
	if (gid_name(hd->gname, &(arcn->sb.st_gid)) < 0)
		arcn->sb.st_gid = (gid_t)asc_ul(hd->gid, sizeof(hd->gid), OCT);
	hd->uname[sizeof(hd->uname) - 1] = '\0';
	if (uid_name(hd->uname, &(arcn->sb.st_uid)) < 0)
		arcn->sb.st_uid = (uid_t)asc_ul(hd->uid, sizeof(hd->uid), OCT);

	/*
	 * set the defaults, these may be changed depending on the file type
	 */
	arcn->ln_name[0] = '\0';
	arcn->ln_nlen = 0;
	arcn->pad = 0;
	arcn->skip = 0;
	arcn->sb.st_rdev = (dev_t)0;

	/*
	 * set the mode and PAX type according to the typeflag in the header
	 */
	switch(hd->typeflag) {
	case FIFOTYPE:
		arcn->type = PAX_FIF;
		arcn->sb.st_mode |= S_IFIFO;
		break;
	case DIRTYPE:
		arcn->type = PAX_DIR;
		arcn->sb.st_mode |= S_IFDIR;
		arcn->sb.st_nlink = 2;

		/*
		 * Some programs that create ustar archives append a '/'
		 * to the pathname for directories. This clearly violates
		 * ustar specs, but we will silently strip it off anyway.
		 */
		if (arcn->name[arcn->nlen - 1] == '/')
			arcn->name[--arcn->nlen] = '\0';
		break;
	case BLKTYPE:
	case CHRTYPE:
		/*
		 * this type requires the rdev field to be set.
		 */
		if (hd->typeflag == BLKTYPE) {
			arcn->type = PAX_BLK;
			arcn->sb.st_mode |= S_IFBLK;
		} else {
			arcn->type = PAX_CHR;
			arcn->sb.st_mode |= S_IFCHR;
		}
		devmajor = (dev_t)asc_ul(hd->devmajor,sizeof(hd->devmajor),OCT);
		devminor = (dev_t)asc_ul(hd->devminor,sizeof(hd->devminor),OCT);
		arcn->sb.st_rdev = TODEV(devmajor, devminor);
		break;
	case SYMTYPE:
	case LNKTYPE:
		if (hd->typeflag == SYMTYPE) {
			arcn->type = PAX_SLK;
			arcn->sb.st_mode |= S_IFLNK;
		} else {
			arcn->type = PAX_HLK;
			/*
			 * so printing looks better
			 */
			arcn->sb.st_mode |= S_IFREG;
			arcn->sb.st_nlink = 2;
		}
#ifndef __APPLE__
		/*
		 * copy the link name
		 */
		arcn->ln_nlen = l_strncpy(arcn->ln_name, hd->linkname,
		    MIN(sizeof(hd->linkname), sizeof(arcn->ln_name) - 1));
		arcn->ln_name[arcn->ln_nlen] = '\0';
#endif /* __APPLE__ */
		break;
#ifdef __APPLE__
	case LONGLINKTYPE:
	case LONGNAMETYPE:
		/*
		 * GNU long link/file; we tag these here and let the
		 * pax internals deal with it -- too ugly otherwise.
		 */
		arcn->type =
		    hd->typeflag == LONGLINKTYPE ? PAX_GLL : PAX_GLF;
		arcn->pad = TAR_PAD(arcn->sb.st_size);
		arcn->skip = arcn->sb.st_size;
		break;
#endif /* __APPLE__ */
	case CONTTYPE:
	case AREGTYPE:
	case REGTYPE:
	default:
		/*
		 * these types have file data that follows. Set the skip and
		 * pad fields.
		 */
		arcn->type = PAX_REG;
		arcn->pad = TAR_PAD(arcn->sb.st_size);
		arcn->skip = arcn->sb.st_size;
		arcn->sb.st_mode |= S_IFREG;
		break;
	}
	return(0);
}

/*
 * ustar_wr()
 *	write a ustar header for the file specified in the ARCHD to the archive
 *	Have to check for file types that cannot be stored and file names that
 *	are too long. Be careful of the term (last arg) to ul_oct, we only use
 *	'\0' for the termination character (this is different than picky tar)
 *	ASSUMED: space after header in header block is zero filled
 * Return:
 *	0 if file has data to be written after the header, 1 if file has NO
 *	data to write after the header, -1 if archive write failed
 */

int
ustar_wr(ARCHD *arcn)
{
	HD_USTAR *hd;
	char *pt;
#ifdef __APPLE__
	char hdblk[sizeof(HD_USTAR)];
	mode_t mode12only;
	int term_char=3;	/* orignal setting */
	term_char=1;		/* To pass conformance tests 274, 301 */
#else
	HD_USTAR hdblk;
#endif /* __APPLE__ */

	/*
	 * check for those file system types ustar cannot store
	 */
	if (arcn->type == PAX_SCK) {
		paxwarn(1, "Ustar cannot archive a socket %s", arcn->org_name);
		return(1);
	}

	/*
	 * check the length of the linkname
	 */
	if (((arcn->type == PAX_SLK) || (arcn->type == PAX_HLK) ||
	    (arcn->type == PAX_HRG)) &&
	    (arcn->ln_nlen > (int)sizeof(hd->linkname))) {
		paxwarn(1, "Link name too long for ustar %s", arcn->ln_name);
#ifdef __APPLE__
		/*
		 * Conformance: test pax:285 wants error code to be non-zero, and
		 * test tar:12 wants error code from pax to be 0
		 */
#endif /* __APPLE__ */
		return(1);
	}

	/*
	 * split the path name into prefix and name fields (if needed). if
	 * pt != arcn->name, the name has to be split
	 */
	if ((pt = name_split(arcn->name, arcn->nlen)) == NULL) {
		paxwarn(1, "File name too long for ustar %s", arcn->name);
		return(1);
	}
#ifdef __APPLE__
	/*
	 * zero out the header so we don't have to worry about zero fill below
	 */
	memset(hdblk, 0, sizeof(hdblk));
	hd = (HD_USTAR *)hdblk;
#else
	hd = &hdblk;
#endif /* __APPLE__ */
	arcn->pad = 0L;

#ifdef __APPLE__
	/* To pass conformance tests 274/301, always set these fields to "zero" */
	ul_oct(0, hd->devmajor, sizeof(hd->devmajor), term_char);
	ul_oct(0, hd->devminor, sizeof(hd->devminor), term_char);
#endif /* __APPLE__*/

	/*
	 * split the name, or zero out the prefix
	 */
	if (pt != arcn->name) {
		/*
		 * name was split, pt points at the / where the split is to
		 * occur, we remove the / and copy the first part to the prefix
		 */
		*pt = '\0';
#ifdef __APPLE__
		strlcpy(hd->prefix, arcn->name, sizeof(hd->prefix));
#else
		l_strncpy(hd->prefix, arcn->name, sizeof(hd->prefix));
#endif /* __APPLE__ */
		*pt++ = '/';
#ifdef __APPLE__
	}
#else
	} else
		memset(hd->prefix, 0, sizeof(hd->prefix));
#endif /* __APPLE__ */

	/*
	 * copy the name part. this may be the whole path or the part after
	 * the prefix.  both the name and prefix may fill the entire field.
	 */
#ifdef __APPLE__
	if (strlen(pt) == sizeof(hd->name)) {	/* must account for name just fits in buffer */
		strncpy(hd->name, pt, sizeof(hd->name));
	} else {
		strlcpy(hd->name, pt, sizeof(hd->name));
	}
#else
	l_strncpy(hd->name, pt, sizeof(hd->name));
#endif /* __APPLE__ */

	/*
	 * set the fields in the header that are type dependent
	 */
	switch(arcn->type) {
	case PAX_DIR:
		hd->typeflag = DIRTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
		memset(hd->devmajor, 0, sizeof(hd->devmajor));
		memset(hd->devminor, 0, sizeof(hd->devminor));
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 3))
#else
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), term_char))
#endif /* __APPLE__ */
			goto out;
		break;
	case PAX_CHR:
	case PAX_BLK:
		if (arcn->type == PAX_CHR)
			hd->typeflag = CHRTYPE;
		else
			hd->typeflag = BLKTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
#endif /* __APPLE__ */
		if (ul_oct((u_long)MAJOR(arcn->sb.st_rdev), hd->devmajor,
#ifdef __APPLE__
		   sizeof(hd->devmajor), term_char) ||
#else
		   sizeof(hd->devmajor), 3) ||
#endif /* __APPLE__ */
		   ul_oct((u_long)MINOR(arcn->sb.st_rdev), hd->devminor,
#ifdef __APPLE__
		   sizeof(hd->devminor), term_char) ||
		   ul_oct((u_long)0L, hd->size, sizeof(hd->size), term_char))
#else
		   sizeof(hd->devminor), 3) ||
		   ul_oct((u_long)0L, hd->size, sizeof(hd->size), 3))
#endif /* __APPLE__ */
			goto out;
		break;
	case PAX_FIF:
		hd->typeflag = FIFOTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
		memset(hd->devmajor, 0, sizeof(hd->devmajor));
		memset(hd->devminor, 0, sizeof(hd->devminor));
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 3))
#else
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), term_char))
#endif /* __APPLE__ */
			goto out;
		break;
	case PAX_SLK:
	case PAX_HLK:
	case PAX_HRG:
		if (arcn->type == PAX_SLK)
			hd->typeflag = SYMTYPE;
		else
			hd->typeflag = LNKTYPE;
#ifdef __APPLE__
		if (strlen(arcn->ln_name) == sizeof(hd->linkname)) {	/* must account for name just fits in buffer */
			strncpy(hd->linkname, arcn->ln_name, sizeof(hd->linkname));
		} else {
			strlcpy(hd->linkname, arcn->ln_name, sizeof(hd->linkname));
		}
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), term_char))
#else
		/* the link name may occupy the entire field in ustar */
		l_strncpy(hd->linkname,arcn->ln_name, sizeof(hd->linkname));
		memset(hd->devmajor, 0, sizeof(hd->devmajor));
		memset(hd->devminor, 0, sizeof(hd->devminor));
		if (ul_oct((u_long)0L, hd->size, sizeof(hd->size), 3))
#endif /* __APPLE__ */
			goto out;
		break;
	case PAX_REG:
	case PAX_CTG:
	default:
		/*
		 * file data with this type, set the padding
		 */
		if (arcn->type == PAX_CTG)
			hd->typeflag = CONTTYPE;
		else
			hd->typeflag = REGTYPE;
#ifndef __APPLE__
		memset(hd->linkname, 0, sizeof(hd->linkname));
		memset(hd->devmajor, 0, sizeof(hd->devmajor));
		memset(hd->devminor, 0, sizeof(hd->devminor));
#endif /* __APPLE__ */
		arcn->pad = TAR_PAD(arcn->sb.st_size);
		if (uqd_oct((u_quad_t)arcn->sb.st_size, hd->size,
#ifdef __APPLE__
		    sizeof(hd->size), term_char)) {
#else
		    sizeof(hd->size), 3)) {
#endif /* __APPLE__*/
			paxwarn(1,"File is too long for ustar %s",arcn->org_name);
			return(1);
		}
		break;
	}

#ifdef __APPLE__
	strncpy(hd->magic, TMAGIC, TMAGLEN);
	strncpy(hd->version, TVERSION, TVERSLEN);
#else
	l_strncpy(hd->magic, TMAGIC, TMAGLEN);
	l_strncpy(hd->version, TVERSION, TVERSLEN);
#endif /* __APPLE__ */

	/*
	 * set the remaining fields. Some versions want all 16 bits of mode
	 * we better humor them (they really do not meet spec though)....
	 */
#ifdef __APPLE__
	if (ul_oct((u_long)arcn->sb.st_uid, hd->uid, sizeof(hd->uid), term_char)) {
		if (uid_nobody == 0) {
			if (uid_name("nobody", &uid_nobody) == -1)
				goto out;
		}
		if (uid_warn != arcn->sb.st_uid) {
			uid_warn = arcn->sb.st_uid;
			paxwarn(1,
			    "Ustar header field is too small for uid %lu, "
			    "using nobody", (u_long)arcn->sb.st_uid);
		}
		if (ul_oct((u_long)uid_nobody, hd->uid, sizeof(hd->uid), term_char))
			goto out;
	}
	if (ul_oct((u_long)arcn->sb.st_gid, hd->gid, sizeof(hd->gid), term_char)) {
		if (gid_nobody == 0) {
			if (gid_name("nobody", &gid_nobody) == -1)
				goto out;
		}
		if (gid_warn != arcn->sb.st_gid) {
			gid_warn = arcn->sb.st_gid;
			paxwarn(1,
			    "Ustar header field is too small for gid %lu, "
			    "using nobody", (u_long)arcn->sb.st_gid);
		}
		if (ul_oct((u_long)gid_nobody, hd->gid, sizeof(hd->gid), term_char))
			goto out;
	}
	/* However, Unix conformance tests do not like MORE than 12 mode bits:
	   remove all beyond (see definition of stat.st_mode structure)		*/
	mode12only = ((u_long)arcn->sb.st_mode) & 0x00000fff;
	if (ul_oct((u_long)mode12only, hd->mode, sizeof(hd->mode), term_char) ||
	    ul_oct((u_long)arcn->sb.st_mtime,hd->mtime,sizeof(hd->mtime),term_char))
#else
	if (ul_oct((u_long)arcn->sb.st_mode, hd->mode, sizeof(hd->mode), 3) ||
	    ul_oct((u_long)arcn->sb.st_uid, hd->uid, sizeof(hd->uid), 3)  ||
	    ul_oct((u_long)arcn->sb.st_gid, hd->gid, sizeof(hd->gid), 3) ||
	    ul_oct((u_long)arcn->sb.st_mtime,hd->mtime,sizeof(hd->mtime),3))
#endif /* __APPLE__ */
		goto out;
#ifdef __APPLE__
	strncpy(hd->uname, name_uid(arcn->sb.st_uid, 0), sizeof(hd->uname));
	strncpy(hd->gname, name_gid(arcn->sb.st_gid, 0), sizeof(hd->gname));
#else
	l_strncpy(hd->uname,name_uid(arcn->sb.st_uid, 0),sizeof(hd->uname));
	l_strncpy(hd->gname,name_gid(arcn->sb.st_gid, 0),sizeof(hd->gname));
#endif /* __APPLE__ */

	/*
	 * calculate and store the checksum write the header to the archive
	 * return 0 tells the caller to now write the file data, 1 says no data
	 * needs to be written
	 */
#ifdef __APPLE__
	if (ul_oct(tar_chksm(hdblk, sizeof(HD_USTAR)), hd->chksum,
	   sizeof(hd->chksum), term_char))
#else
	if (ul_oct(tar_chksm((char *)&hdblk, sizeof(HD_USTAR)), hd->chksum,
	   sizeof(hd->chksum), 3))
#endif /* __APPLE__ */
		goto out;
	if (wr_rdbuf((char *)&hdblk, sizeof(HD_USTAR)) < 0)
		return(-1);
	if (wr_skip((off_t)(BLKMULT - sizeof(HD_USTAR))) < 0)
		return(-1);
	if ((arcn->type == PAX_CTG) || (arcn->type == PAX_REG))
		return(0);
	return(1);

    out:
    	/*
	 * header field is out of range
	 */
	paxwarn(1, "Ustar header field is too small for %s", arcn->org_name);
	return(1);
}

/*
 * name_split()
 *	see if the name has to be split for storage in a ustar header. We try
 *	to fit the entire name in the name field without splitting if we can.
 *	The split point is always at a /
 * Return
 *	character pointer to split point (always the / that is to be removed
 *	if the split is not needed, the points is set to the start of the file
 *	name (it would violate the spec to split there). A NULL is returned if
 *	the file name is too long
 */

static char *
name_split(char *name, int len)
{
	char *start;

	/*
	 * check to see if the file name is small enough to fit in the name
	 * field. if so just return a pointer to the name.
	 */
	if (len <= TNMSZ)
		return(name);
#ifdef __APPLE__
	/*
	 * The strings can fill the complete name and prefix fields
	 * without a NUL terminator.
	 */
	if (len > (TPFSZ + TNMSZ + 1))
#else
	if (len > TPFSZ + TNMSZ)
#endif /* __APPLE__ */
		return(NULL);

	/*
	 * we start looking at the biggest sized piece that fits in the name
	 * field. We walk forward looking for a slash to split at. The idea is
	 * to find the biggest piece to fit in the name field (or the smallest
	 * prefix we can find)
	 */
#ifdef __APPLE__
	/*
	 * the -1 is correct the biggest piece would
	 * include the slash between the two parts that gets thrown away
	 */
	start = name + len - TNMSZ - 1;
	if ((*start == '/') && (start == name))
		++start;	/* 101 byte paths with leading '/' are dinged otherwise */
#else
	start = name + len - TNMSZ;
#endif /* __APPLE__ */
	while ((*start != '\0') && (*start != '/'))
		++start;

	/*
	 * if we hit the end of the string, this name cannot be split, so we
	 * cannot store this file.
	 */
	if (*start == '\0')
		return(NULL);
	len = start - name;

	/*
	 * NOTE: /str where the length of str == TNMSZ can not be stored under
	 * the p1003.1-1990 spec for ustar. We could force a prefix of / and
	 * the file would then expand on extract to //str. The len == 0 below
	 * makes this special case follow the spec to the letter.
	 */
	if ((len > TPFSZ) || (len == 0))
		return(NULL);

	/*
	 * ok have a split point, return it to the caller
	 */
	return(start);
}

#ifdef __APPLE__
static size_t
expandname(char *buf, size_t len, char **gnu_name, const char *name,
    size_t name_len)
{
	size_t nlen;

	if (*gnu_name) {
		/* *gnu_name is NUL terminated */
		if ((nlen = strlcpy(buf, *gnu_name, len)) >= len)
			nlen = len - 1;
		free(*gnu_name);
		*gnu_name = NULL;
	} else {
		if (name_len < len) {
			/* name may not be null terminated: it might be as big as the
			   field,  so copy is limited to the max size of the header field */
			if ((nlen = strlcpy(buf, name, name_len+1)) >= name_len+1)
				nlen = name_len;
		} else {
			if ((nlen = strlcpy(buf, name, len)) >= len)
				nlen = len - 1;
		}
	}
	return(nlen);
}

#endif /* __APPLE__ */
