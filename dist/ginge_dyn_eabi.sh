#!/bin/sh

root=$1
shift

loader="${root}lib/ld-linux.so.2"
export LD_LIBRARY_PATH="${root}lib"
export LD_PRELOAD="${root}ginge_dyn"

export GINGE_ROOT="${root}"

exec "${loader}" "$@"
