#!/bin/sh

set -e

rm -rf ${out}
mkdir ${out}
cp gp2xmenu/gp2xmenu${tag} ${out}/gp2xmenu
cp -r gp2xmenu/gp2xmenu_data ${out}/
cp prep/ginge_prep${tag} ${out}/ginge_prep
cp loader/ginge_dyn${tag} ${out}/ginge_dyn
cp loader/ginge_sloader${tag} ${out}/ginge_sloader
cp readme.txt ${out}/

