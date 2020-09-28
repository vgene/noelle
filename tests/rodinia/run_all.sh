#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Need argument for variant"
  exit 1
fi

bmarks=$(ls *.bc)

mkdir -p "$1"

for x in $bmarks; do
  fixed_bc="$(echo $x | cut -f1 -d".")_split.bc"
  pdgstats="$(echo $x | cut -f1 -d".").pdg"
  loopstats="$(echo $x | cut -f1 -d".").loops"
  echo "Generating files for $x"
  ./get_cgo.sh . $x $pdgstats
  noelle-load $x -talkdown-split-basic-blocks -o $fixed_bc
  noelle-loop-stats $fixed_bc > $loopstats 2>&1

  cp $pdgstats "$1"
  cp $loopstats "$1"
done
