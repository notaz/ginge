#!/bin/sh

set -e

outc=out_caanoo
export out=${outc}/ginge
# all same as wiz, except menu
export tag=_wiz

rm -rf ${out}
dist/make_cmn.sh
cp gp2xmenu/gp2xmenu_caanoo ${out}/gp2xmenu
mkdir -p ${out}/tools ${out}/lib
cp dist/ginge.gpe ${out}/
cp dist/ginge.ini ${outc}/
cp dist/ginge26.png ${out}/ginge.png
cp dist/ginge_banner.png ${out}/
cp dist/ginge_dyn_eabi.sh ${out}/ginge_dyn.sh
cp tools/cramfsck_eabi ${out}/tools/cramfsck
cp tools/warm_2.6.24.ko ${out}/tools/
cp -r lib ${out}/

cd ${outc}/
rm ../ginge_caanoo.zip 2> /dev/null || true
zip -9r ../ginge_caanoo.zip *

