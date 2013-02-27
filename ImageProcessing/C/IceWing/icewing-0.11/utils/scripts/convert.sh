#!/bin/sh

FILES=`find -name 'icip*.ndr'`

for file in $FILES; do
    echo Converting $file...
    bild2pnm $file img
    mv img00.ppm `basename $file .ndr`.ppm
done
