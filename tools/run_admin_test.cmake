# Checks the staff-account tool: creates a staff account in a temp data dir,
# refuses a duplicate, a short password, and refuses while the server runs.
# Usage: cmake -DEXE=... -DWORK=... -P tools/run_admin_test.cmake
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/pw.txt" "staffpass123\nstaffpass123\n")
file(WRITE "${WORK}/short.txt" "short\nshort\n")
file(WRITE "${WORK}/mismatch.txt" "staffpass123\nstaffpass999\n")

execute_process(COMMAND "${EXE}" --data "${WORK}" add-staff cafe.staff "Cafeteria Staff"
                INPUT_FILE "${WORK}/pw.txt" OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "add-staff failed: ${rc}")
endif()
file(READ "${WORK}/users.txt" users)
string(FIND "${users}" "cafe.staff||staff|Cafeteria Staff||$argon2id$" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR "users.txt does not contain the hashed staff account:\n${users}")
endif()
string(FIND "${users}" "staffpass123" plain)
if(NOT plain EQUAL -1)
  message(FATAL_ERROR "plain-text password found in users.txt")
endif()

foreach(case "pw.txt;duplicate" "short.txt;short password" "mismatch.txt;passwords differ")
  list(GET case 0 input)
  list(GET case 1 label)
  execute_process(COMMAND "${EXE}" --data "${WORK}" add-staff other.staff "Other"
                  INPUT_FILE "${WORK}/${input}" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
  if(label STREQUAL "duplicate")
    execute_process(COMMAND "${EXE}" --data "${WORK}" add-staff cafe.staff "Again"
                    INPUT_FILE "${WORK}/pw.txt" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
  endif()
  if(rc EQUAL 0)
    message(FATAL_ERROR "expected failure for ${label}")
  endif()
endforeach()

file(WRITE "${WORK}/server.lock" "4000000
")
execute_process(COMMAND "${EXE}" --data "${WORK}" add-staff stale.staff "Stale Lock"
                INPUT_FILE "${WORK}/pw.txt" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "a stale lock (dead PID) must not block add-staff")
endif()
file(WRITE "${WORK}/server.lock" "4
")  # PID 4 is always alive
execute_process(COMMAND "${EXE}" --data "${WORK}" add-staff locked.staff "Locked"
                INPUT_FILE "${WORK}/pw.txt" OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE rc)
if(rc EQUAL 0)
  message(FATAL_ERROR "add-staff must refuse while server.lock exists")
endif()

# add-person: university staff without a university email.
file(REMOVE "${WORK}/server.lock")
file(MAKE_DIRECTORY "${WORK}/config")
file(WRITE "${WORK}/config/university_list.csv"
     "id,email,role,name,department\n1072520005101030,ayon@uni.example,student,Ayon Das,CSE\n3000000000000001,,employee,Rahim Driver,Transport\n")
file(WRITE "${WORK}/driver.txt" "driverpass1\ndriverpass1\n")
execute_process(COMMAND "${EXE}" --data "${WORK}" --config "${WORK}/config" add-person 3000000000000001 rahim@example.com
                INPUT_FILE "${WORK}/driver.txt" OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "add-person failed: ${rc}")
endif()
file(READ "${WORK}/users.txt" users)
string(FIND "${users}" "rahim@example.com|3000000000000001|employee|Rahim Driver|Transport|$argon2id$" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR "users.txt does not contain the hashed university staff account:\n${users}")
endif()
string(FIND "${users}" "driverpass1" plain)
if(NOT plain EQUAL -1)
  message(FATAL_ERROR "plain-text password found in users.txt")
endif()
foreach(case "1072520005101030;other@example.com;driver.txt;student ID"
             "3000000000000001;again@example.com;driver.txt;same ID twice"
             "3000000000000001;new@example.com;short.txt;short password")
  list(GET case 0 pid)
  list(GET case 1 pemail)
  list(GET case 2 input)
  list(GET case 3 label)
  execute_process(COMMAND "${EXE}" --data "${WORK}" --config "${WORK}/config" add-person ${pid} ${pemail}
                  INPUT_FILE "${WORK}/${input}" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
  if(rc EQUAL 0)
    message(FATAL_ERROR "add-person: expected failure for ${label}")
  endif()
endforeach()
file(WRITE "${WORK}/server.lock" "4
")
execute_process(COMMAND "${EXE}" --data "${WORK}" --config "${WORK}/config" add-person 3000000000000001 locked@example.com
                INPUT_FILE "${WORK}/driver.txt" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
if(rc EQUAL 0)
  message(FATAL_ERROR "add-person must refuse while server.lock exists")
endif()
message("admin_tool: OK")
