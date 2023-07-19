#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2017-2021, Silicon Laboratories, Inc.

SRC_DIR=$1
VERSION_FILE=$2
GIT_LABEL="$(git -C $SRC_DIR describe --tags --dirty --match "*v[0-9]*" || echo '<unknown version>')"

cat << EOF > $VERSION_FILE.tmp
/* SPDX-License-Identifier: GPL-2.0 */
/* THIS FILE IS AUTOMATICALLY GENERATED. DO NOT EDIT! */
#include <stdint.h>
#include "common/version.h"

/*
 * wsbrd API versions:
 *
 * 0.2.0
 * - Pop the retry_per_rate array and the successful phy mode id
 *   in SPINEL_CMD_PROP_IS/SPINEL_PROP_STREAM_STATUS that were
 *   used to send the data, necessary for mode switch fallback
 *
 * 0.1.0
 * - Pop ACK request, frame pending, and PAN ID suppression in
 *   PROP_IS/STREAM_RAW, for MAC frame reconstruction
 */

const char *version_daemon_str = "${GIT_LABEL}";
uint32_t version_daemon_api = VERSION(0, 2, 0);

const char *version_hwsim_str = "hwsim-${GIT_LABEL}";
uint32_t version_hwsim = VERSION(0, 0, 0);
uint32_t version_hwsim_api = VERSION(0, 18, 0);
EOF

if cmp -s $VERSION_FILE $VERSION_FILE.tmp
then
    rm -f $VERSION_FILE.tmp
else
    mv -f $VERSION_FILE.tmp  $VERSION_FILE
fi
