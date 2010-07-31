#!/bin/sh

export FBDEV=/dev/fb1
ofbset -fb $FBDEV -pos 80 0 -size 640 480 -mem 614400 -en 1
fbset -fb $FBDEV -g 320 240 320 480 16

# make it runnable from ssh
if [ -z "$DISPLAY" ]; then
export DISPLAY=:0
fi

./gp2xmenu
  
ofbset -fb $FBDEV -pos 0 0 -size 0 0 -mem 0 -en 0
