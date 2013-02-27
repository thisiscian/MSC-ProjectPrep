#!/bin/sh

IPRemBack="/vol/vita/src/segmentation/regions/classPixel/imgPolynom/IPRemoveBack"
IPRemBackMASK="/vol/vita/src/segmentation/regions/classPixel/imgPolynom/divers/IPRemoveBack.mask"

# Directoryname und Praefix der Filenamen
# Aus Reihenfolge in String ergeben sich die Klassenlabel 0 bis n-1
KLASSE="hand hgrund"

# Ein Teil aus dem Hintergrund, falls im Klassendirectory vorhanden,
# werden Pixel aus diesem Trainingsbild ausmaskiert
TRAIN="blau_RGB.ppm"

# Erzeugtes Script, das Tmp-Images und Momentenmatrizen spaeter loeschen kann
LOESCH="loesch_tmp.sh"

# Erzeugte Liste aller Trainingsbilder mit den dazugehoerigen Klassen
FLISTE="stichprobe.txt"

anzklass="0"
for klasse in $KLASSE; do
    anzklass=`expr $anzklass + 1`
done

echo >$LOESCH
chmod +x $LOESCH
echo >$FLISTE
klasse_nr="0"

for klasse in $KLASSE; do
    echo cd $klasse
    cd $klasse
    YUV=`find -maxdepth 1 -name "$klasse*[0-9].ppm"`
    for yuv in $YUV; do
        yuv=`basename $yuv`			# Entfernen von "./"
        echo "File: $yuv Klasse: $klasse($klasse_nr)"
	if [ -e $TRAIN ]; then
	    rgb="`basename $yuv .ppm`_RGB.ppm"

	    echo "  AVConv -c YUV_PIX:RGB $yuv $rgb"
	    AVConv -c YUV_PIX:RGB $yuv $rgb

	    echo "  $IPRemBack -A $rgb $TRAIN 1E-10 0.99 $IPRemBackMASK"
	    $IPRemBack -A $rgb $TRAIN 1E-10 0.99 $IPRemBackMASK
	    echo "  $IPRemBack -M $yuv $rgb.0 0"
	    $IPRemBack -M $yuv $rgb.0 0

	    echo >>../$LOESCH "rm $klasse/$rgb"
	    echo >>../$LOESCH "rm $klasse/$rgb.0"
	    echo >>../$LOESCH "rm $klasse/$yuv.0"
	    echo >>../$LOESCH "rm $klasse/$yuv.0.mom"

	    echo >>../$FLISTE `pwd`/$yuv.0 $klasse_nr
	else
	    echo >>../$LOESCH "rm $klasse/$yuv.mom"
	    echo >>../$FLISTE `pwd`/$yuv $klasse_nr
	fi
    done
    echo cd ..
    cd ..
    klasse_nr=`expr $klasse_nr + 1`
done
