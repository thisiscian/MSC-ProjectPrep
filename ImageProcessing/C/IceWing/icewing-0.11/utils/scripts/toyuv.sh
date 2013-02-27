#!/bin/sh

if test -z "$1"; then
    cat << EOF
Usage: toyuv.sh images ...
  Convert PPM images from RGB to YUV and safe them in the current directory
EOF
    exit 1
fi

for file in $@; do
    dest=`basename $file`
    echo $file "->" $dest
    AVConv -cRGB:YUV_PIX $file $dest
done
