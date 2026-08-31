# Runs the CLI over every sample and diffs against baseline.txt.
file(GLOB SAMPLES "${DIR}/samples/*.dn2pst")
list(SORT SAMPLES)
execute_process(COMMAND ${EXE} -t ${SAMPLES}
                OUTPUT_VARIABLE OUT RESULT_VARIABLE RC)
if(NOT RC EQUAL 0)
  message(FATAL_ERROR "dn2pst exited with ${RC}")
endif()
file(READ "${DIR}/baseline.txt" EXPECTED)
if(NOT OUT STREQUAL EXPECTED)
  file(WRITE "${DIR}/baseline.actual.txt" "${OUT}")
  message(FATAL_ERROR
    "parameter table changed; wrote baseline.actual.txt for diffing")
endif()
message(STATUS "baseline matches")
