#!/bin/sh

pnd_make=$HOME/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

set -e

dist/make_cmn.sh out_pnd
mkdir -p out_pnd/tools
cp dist/ginge.sh out_pnd/
cp dist/ginge_dyn_eabi.sh out_pnd/ginge_dyn.sh
cp tools/cramfsck_eabi out_pnd/tools/cramfsck
cp -r lib out_pnd/

$pnd_make -p ginge.pnd -d out_pnd -x dist/ginge.pxml -c -i dist/ginge60.png
