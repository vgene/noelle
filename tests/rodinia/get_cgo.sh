#!/bin/bash

function parserTotal {
  awk '
    {
      if (  ($1 == "Number") && ($3 == "potential") && ($4 == "memory") && ($5 == "dependences:")  ){
        printf("%d ", $6);
      }
    }' $tmpFile ;
}

function parser {
  awk '
    {
      if (  ($1 == "Number") && ($3 == "memory") && ($4 == "dependences:")  ){
        printf("%d ", $5);
      }
    }' $tmpFile ;
}

if test $# -lt 3; then
  echo "USAGE: `basename $0` DIR FILENAME OUTPUT_FILE" ;
  exit 1;
fi

dirToAnalyze="`realpath $1`" ;
IRFile="$2" ;
noelleOutputFile="$3" ;

# Prepare the files
rm -f $noelleOutputFile ;
tmpFile=`mktemp` ;
tmpFile2=`mktemp` ;
pdgEmbedFile=`mktemp` ;

for i in `find ${dirToAnalyze} -name ${IRFile}` ; do

  # Fetch the name of the benchmark
  benchmarkDirName=`dirname $i` ;
  benchmarkName=`basename $benchmarkDirName` ;

  # Get the results for NOELLE
  echo -n "$benchmarkName " >> $noelleOutputFile ;
  echo -n "    FAILED" ;
  noelle-meta-pdg-embed ${i} -o $pdgEmbedFile ;
  noelle-pdg-stats ${pdgEmbedFile} &> $tmpFile ;
  parserTotal >> $noelleOutputFile ;
  parser >> $noelleOutputFile ;

  # Clean the metadata
  noelle-meta-pdg-clean $i $tmpFile2 ;

  # Get the results for LLVM
  noelle-meta-pdg-embed -noelle-disable-pdg-allocaa -noelle-disable-pdg-reaching-analysis -noelle-disable-pdg-svf -noelle-disable-loop-aware-dependence-analyses ${tmpFile2} -o $tmpFile2 ;
  noelle-pdg-stats -noelle-disable-pdg-allocaa -noelle-disable-pdg-reaching-analysis -noelle-disable-pdg-svf -noelle-disable-loop-aware-dependence-analyses ${tmpFile2} &> $tmpFile ;
  parser >> $noelleOutputFile ;
  echo "" >> $noelleOutputFile ;

done

rm $tmpFile $tmpFile2 $pdgEmbedFile;
