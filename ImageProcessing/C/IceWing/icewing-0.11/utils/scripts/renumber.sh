#!/bin/sh

FILES=`ls -1 *[0-9].*ppm`

for file in $FILES; do
    nr=`echo $file |sed "s/[a-zA-Z._]*//g"`
    nr_dst=`echo $nr|awk '{printf "%03d\n",$1-61}'`
    file2=`echo $file |sed "s/[0-9]*[°.]/$nr_dst./"`
    echo "mv $file $file2"
    mv $file $file2
done
