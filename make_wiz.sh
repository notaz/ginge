#!/bin/sh

set -e

dist/make_cmn.sh out_wiz
mkdir -p out_wiz/tools
cp dist/ginge.gpe out_wiz/
cp dist/ginge32.png out_wiz/ginge.png
cp dist/ginge_dyn_oabi.sh out_wiz/ginge_dyn.sh
cp tools/cramfsck_oabi out_wiz/tools/cramfsck
cp tools/warm_2.6.24.ko out_wiz/tools/

dd if=/dev/zero of=out_wiz/swapfile bs=1M count=16
