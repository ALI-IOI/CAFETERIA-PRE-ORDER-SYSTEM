# Runs the console demo with a scripted stdin and checks that every line of the
# expected file appears in the output, in order.
# Usage: cmake -DEXE=... -DSCRIPT=... -DEXPECT=... -P tools/run_console_script.cmake
execute_process(COMMAND "${EXE}" INPUT_FILE "${SCRIPT}" OUTPUT_VARIABLE out RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "console demo exited with ${rc}")
endif()

file(STRINGS "${EXPECT}" expected)
set(rest "${out}")
foreach(line IN LISTS expected)
  string(FIND "${rest}" "${line}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "expected line not found (in order): ${line}")
  endif()
  string(LENGTH "${line}" len)
  math(EXPR next "${pos} + ${len}")
  string(SUBSTRING "${rest}" ${next} -1 rest)
endforeach()
message("console_demo: all expected lines found in order")
