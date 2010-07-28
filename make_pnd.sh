#!/bin/sh

pnd_make=$HOME/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

set -e

rm -rf out
mkdir out
cp gp2xmenu/run.sh out/ginge.sh
cp gp2xmenu/gp2xmenu out/
cp -r gp2xmenu/gp2xmenu_data out/
cp prep/ginge_prep out/
cp loader/ginge_* out/
cp -r tools out/
cp -r lib out/
cp readme.txt out/

$pnd_make -p ginge.pnd -d out -x ginge.pxml -c -i ginge.png
