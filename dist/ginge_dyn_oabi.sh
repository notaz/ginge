#!/bin/sh

root=$1
shift

#export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:${root}lib"
export LD_PRELOAD="${root}ginge_dyn"

export GINGE_ROOT="${root}"

exec "$@"
