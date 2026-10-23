# Fails if any STL container is used in the directories that must use the
# hand-built array / linked list / queue / stack (proposal section 7).
# Usage: cmake -DROOT=<project root> -P tools/check_no_stl.cmake
set(DIRS src/ds src/core src/auth src/storage src/console src/email)
set(PATTERN "#include[ \t]*<(vector|list|forward_list|queue|stack|deque|map|set|unordered_map|unordered_set|array)>|std::(vector|list|forward_list|queue|priority_queue|stack|deque|map|multimap|set|multiset|unordered_map|unordered_set|array)[ \t]*<")

set(found "")
foreach(dir ${DIRS})
  file(GLOB_RECURSE files "${ROOT}/${dir}/*.h" "${ROOT}/${dir}/*.cpp")
  foreach(f ${files})
    file(STRINGS "${f}" lines REGEX "${PATTERN}")
    foreach(line ${lines})
      list(APPEND found "${f}: ${line}")
    endforeach()
  endforeach()
endforeach()

if(found)
  foreach(item ${found})
    message("STL container found: ${item}")
  endforeach()
  message(FATAL_ERROR "no_stl_guard: STL containers are not allowed in ${DIRS}")
endif()
message("no_stl_guard: OK - no STL containers in ${DIRS}")
