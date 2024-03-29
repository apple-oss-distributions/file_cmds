#!/bin/sh
#
# Copyright (c) 2022 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@

: ${ATF_SH="atf-sh"}
# Light wrapper for badmtime; we can't specify a cleanup/post command in our
# test plist, so we'll just run both here.
scriptdir=$(dirname $(realpath "$0"))

trap "${ATF_SH} "$scriptdir"/ls_tests.sh badmtime:cleanup" EXIT
${ATF_SH} "$scriptdir"/ls_tests.sh -s "$scriptdir" \
    -r ls_tests.sh.badmtime.results.txt badmtime
