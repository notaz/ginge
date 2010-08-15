#!/bin/sh

pnd_make=$HOME/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

set -e

out=out_pnd

dist/make_cmn.sh ${out}
mkdir -p ${out}/tools
cp dist/ginge.sh ${out}/
cp dist/ginge_dyn_eabi.sh ${out}/ginge_dyn.sh
cp prep/ginge_prep_pnd ${out}/ginge_prep
cp tools/cramfsck_eabi ${out}/tools/cramfsck
cp -r lib ${out}/

$pnd_make -p ginge.pnd -d ${out} -x dist/ginge.pxml -c -i dist/ginge60.png
