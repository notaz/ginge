#!/bin/sh

pnd_make=$HOME/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

set -e

export out=out_pnd
export tag=_pnd

dist/make_cmn.sh
cp gp2xmenu/gp2xmenu${tag} ${out}/gp2xmenu
mkdir -p ${out}/tools
cp dist/ginge.sh ${out}/
cp dist/ginge_dyn_eabi.sh ${out}/ginge_dyn.sh
cp tools/cramfsck_eabi ${out}/tools/cramfsck
cp -r lib ${out}/

$pnd_make -p ginge.pnd -d ${out} -x dist/ginge.pxml -c -i dist/ginge60.png
