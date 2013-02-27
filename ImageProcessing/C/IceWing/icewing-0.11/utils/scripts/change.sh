#!/bin/sh

FILES=`find -name '*.[hcC]'`

if test -z "$1"; then
    cat << EOF
Usage: change.sh sed-script
  Apply sed-scrip to all *.[hcC] files recursively.
EOF
    exit 1
fi

for file in $FILES; do
    echo $file
    sed "$1" $file > tt
    mv tt $file
done
