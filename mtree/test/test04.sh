#!/bin/sh
#
# Copyright (c) 2003 Dan Nelson
# All rights reserved.
#
# Please see src/share/examples/etc/bsd-style-copyright.
#
# $FreeBSD: src/usr.sbin/mtree/test/test04.sh,v 1.1 2003/11/13 11:02:57 phk Exp $
#

set -e

TMP=/tmp/mtree.$$
rm -rf ${TMP}

mkdir -p ${TMP}/mr ${TMP}/mt
mkdir ${TMP}/mr/a
mkdir ${TMP}/mr/b
mkdir ${TMP}/mt/a
mkdir ${TMP}/mt/b
touch ${TMP}/mt/z

mtree -c -p ${TMP}/mr > ${TMP}/_r
mtree -c -p ${TMP}/mt > ${TMP}/_t

if mtree -f ${TMP}/_r -f ${TMP}/_t > ${TMP}/_ ; then
	echo "ERROR wrong exit on difference" 1>&2
	exit 1
fi

if [ `wc -l < ${TMP}/_` -ne 7 ] ; then
	echo "ERROR spec/spec compare generated wrong output" 1>&2
	exit 1
fi

if mtree -f ${TMP}/_t -f ${TMP}/_r > ${TMP}/_ ; then
	echo "ERROR wrong exit on difference" 1>&2
	exit 1
fi

if [ `wc -l < ${TMP}/_` -ne 7 ] ; then
	echo "ERROR spec/spec compare generated wrong output" 1>&2
	exit 1
fi

rm -rf ${TMP}
exit 0

