#!/bin/sh

FPATH="/vol/vita/src/segmentation/regions/classPixel/imgPolynom"

# Directoryname und Praefix der Filenamen
# Aus Reihenfolge in String ergeben sich die Klassenlabel 0 bis n-1
KLASSE="hand hgrund"

# Liste aller Trainingsbilder mit den dazugehoerigen Klassen
FLISTE="stichprobe.txt"

LOOKUP="lookup.dat"
LOOKUP_CONF="lookup.conf"
HISTOGRAM="/tmp/klasse"

CONFIDENCE="confidence.dat"
MATRIX="matrix.dat"

anzklass="0"
for klasse in $KLASSE; do
    anzklass=`expr $anzklass + 1`
done

echo $FPATH/IPCalcMoments -d 6 -k $anzklass -m $FLISTE -n 64 -a 1
$FPATH/IPCalcMoments -d 6 -k $anzklass -m $FLISTE -n 64 -a 1

echo $FPATH/IPTrainMoments -d 6 -k $anzklass -m $FLISTE -o $MATRIX
$FPATH/IPTrainMoments -d 6 -k $anzklass -m $FLISTE -o $MATRIX

if [ ! -z "$LOOKUP_CONF" ]; then
    if [ ! -z "$HISTOGRAM" ]; then
	echo $FPATH/IPCalcConf -m $FLISTE -p $MATRIX -o $CONFIDENCE -h $HISTOGRAM -a 1
	$FPATH/IPCalcConf -m $FLISTE -p $MATRIX -o $CONFIDENCE -h $HISTOGRAM -a 1
    else
	echo $FPATH/IPCalcConf -m $FLISTE -p $MATRIX -o $CONFIDENCE -a 1
	$FPATH/IPCalcConf -m $FLISTE -p $MATRIX -o $CONFIDENCE -a 1
    fi
    echo $FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP_CONF -c $CONFIDENCE
    $FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP_CONF -c $CONFIDENCE

    if [ ! -z "$LOOKUP" ]; then
	echo $FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP
	$FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP
    fi
else
    echo $FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP
    $FPATH/IPCreateLookup -p $MATRIX -v 4 -o $LOOKUP
fi
