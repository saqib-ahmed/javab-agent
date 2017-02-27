#! /bin/bash


JAVAB="/home/saqib/Downloads/JAVAB"

echo $JAVAB
echo $JAVAB_THREADS
rm Threading.class Threading_*
mv Threading.old Threading.class
$JAVAB/javab -op Threading.class 2> /dev/null > /dev/null & 
