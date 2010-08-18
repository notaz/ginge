#!/bin/sh

set -e

export out=out_wiz
export tag=_wiz

dist/make_cmn.sh
mkdir -p ${out}/tools ${out}/lib
cp dist/ginge.gpe ${out}/
cp dist/ginge32.png ${out}/ginge.png
cp dist/ginge_dyn_oabi.sh ${out}/ginge_dyn.sh
cp lib/libSDL-1.2.so.0.7.0 ${out}/lib/libSDL-1.2.so.0
cp tools/cramfsck_oabi ${out}/tools/cramfsck
cp tools/warm_2.6.24.ko ${out}/tools/

dd if=/dev/zero of=${out}/swapfile bs=1M count=16

cd ${out}
rm ../ginge_wiz.zip 2> /dev/null || true
zip -9r ../ginge_wiz.zip *
