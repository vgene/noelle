#!/bin/bash

function runningTestsWrapper {
  local optionsToUse="$@" ;

  if test "${optionsToUse}" == "" ; then
    runningTests "Testing the default configuration" "-noelle-verbose=3" ;
  else
    runningTests "Testing with \"${optionsToUse}\"" "-noelle-verbose=3 ${optionsToUse}" ;
  fi

  return ;
}

function runningTests {
  if [ -z "$3" ]; then
    output_file="compiler_output.txt"
  else
    output_file="$3"
  fi
  echo $1 ;
  # Clean
  # make clean > /dev/null ;
	rm parallelized test_dswp.bc test_dswp_unoptimized.bc > /dev/null

  # Compile
  make PARALLELIZATION_OPTIONS="$2" >> $output_file 2>&1 ;

  # Generate the input
  make input.txt &> /dev/null ;

  # Baseline
  ./baseline `cat input.txt` &> output_baseline.txt ;

  # Transformation
  ./parallelized `cat input.txt` &> output_parallelized.txt ;

  # Check the output ;
  cmp output_baseline.txt output_parallelized.txt &> /dev/null ;
}

export PATH=$HOME/noelle/install/bin:$PATH

# Test enablers
# runningTestsWrapper -noelle-disable-helix -noelle-disable-dswp -noelle-disable-doall ;

# Test parallelization techniques
# runningTestsWrapper
rm compiler_output_with_talkdown compiler_output_no_talkdown
# runningTests "Testing with Talkdown" "-noelle-verbose=3" "compiler_output_with_talkdown"
# runningTests "Testing without Talkdown" "-noelle-verbose=3 -noelle-talkdown-disable" "compiler_output_no_talkdown"
runningTests "Testing with Talkdown" "" "compiler_output_with_talkdown"
runningTests "Testing without Talkdown" "-noelle-talkdown-disable" "compiler_output_no_talkdown"
exit 0;

runningTestsWrapper -dswp-force -noelle-disable-helix ;
runningTestsWrapper -dswp-force -noelle-disable-helix -dswp-no-scc-merge ;

runningTestsWrapper -dswp-force -noelle-disable-dswp ;
runningTestsWrapper -dswp-force -noelle-disable-dswp -dswp-no-scc-merge ;

runningTestsWrapper -dswp-force -noelle-disable-doall ;
runningTestsWrapper -dswp-force -noelle-disable-doall -dswp-no-scc-merge ;

runningTestsWrapper -dswp-force -noelle-disable-doall -noelle-disable-helix ;
runningTestsWrapper -dswp-force -noelle-disable-doall -noelle-disable-helix -dswp-no-scc-merge ;

runningTestsWrapper -dswp-force -noelle-disable-doall -noelle-disable-dswp ;
runningTestsWrapper -dswp-force -noelle-disable-doall -noelle-disable-dswp -dswp-no-scc-merge ;

exit 0;
