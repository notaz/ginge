#!/bin/sh

set -e

out=out_wiz

dist/make_cmn.sh ${out}
mkdir -p ${out}/tools ${out}/lib
cp dist/ginge.gpe ${out}/
cp dist/ginge32.png ${out}/ginge.png
cp dist/ginge_dyn_oabi.sh ${out}/ginge_dyn.sh
cp prep/ginge_prep_wiz ${out}/ginge_prep
cp lib/libSDL-1.2.so.0.7.0 ${out}/lib/libSDL-1.2.so.0
cp tools/cramfsck_oabi ${out}/tools/cramfsck
cp tools/warm_2.6.24.ko ${out}/tools/

dd if=/dev/zero of=${out}/swapfile bs=1M count=16

cd ${out}
zip -9r ../ginge_wiz.zip *
