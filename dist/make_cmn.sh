#!/bin/sh

set -e

if [ -z "$1" ]; then
	echo "usage: $0 <out_dir>"
	exit 1
fi

rm -rf $1
mkdir $1
cp gp2xmenu/gp2xmenu $1/
cp -r gp2xmenu/gp2xmenu_data $1/
cp prep/ginge_prep $1/
cp loader/ginge_dyn $1/
cp loader/ginge_sloader $1/
cp readme.txt $1/

