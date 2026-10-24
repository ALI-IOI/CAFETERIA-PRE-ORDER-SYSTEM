cmake_minimum_required(VERSION 3.16)
project(CafeteriaPreOrder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

if(WIN32)
  # cpp-httplib uses Windows 8+ APIs (CreateFile2); target Windows 10.
  add_compile_definitions(_WIN32_WINNT=0x0A00)
  # Self-contained .exe files: no MinGW runtime DLLs needed at run time.
  add_link_options(-static)
endif()

set(SODIUM_DIR ${CMAKE_SOURCE_DIR}/third_party/libsodium/libsodium-win64)
set(HTTPLIB_DIR ${CMAKE_SOURCE_DIR}/third_party/httplib)

# ---- core library: data structures, rules, storage, auth, email --------------
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
  src/ds/*.cpp src/core/*.cpp src/storage/*.cpp src/auth/*.cpp src/email/*.cpp)
add_library(cafe_core STATIC ${CORE_SOURCES} src/version.cpp)
target_include_directories(cafe_core PUBLIC src ${SODIUM_DIR}/include)
target_compile_definitions(cafe_core PUBLIC SODIUM_STATIC)
target_link_libraries(cafe_core PUBLIC ${SODIUM_DIR}/lib/libsodium.a)
if(WIN32)
  target_link_libraries(cafe_core PUBLIC ws2_32 bcrypt crypt32)
endif()

# ---- programs ----------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/src/console/console_demo.cpp)
  add_executable(cafeteria_console src/console/console_demo.cpp)
  target_link_libraries(cafeteria_console PRIVATE cafe_core)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/src/console/cafeteria_admin.cpp)
  add_executable(cafeteria_admin src/console/cafeteria_admin.cpp)
  target_link_libraries(cafeteria_admin PRIVATE cafe_core)
endif()
file(GLOB SERVER_SOURCES CONFIGURE_DEPENDS src/server/*.cpp)
if(SERVER_SOURCES)
  add_executable(cafeteria_server ${SERVER_SOURCES})
  target_include_directories(cafeteria_server PRIVATE ${HTTPLIB_DIR})
  target_link_libraries(cafeteria_server PRIVATE cafe_core)
endif()

# ---- tests -------------------------------------------------------------------
enable_testing()
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE cafe_core)
add_test(NAME unit_tests COMMAND unit_tests)

add_test(NAME no_stl_guard
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tools/check_no_stl.cmake)

if(EXISTS ${CMAKE_SOURCE_DIR}/tests/console_script.txt AND TARGET cafeteria_console)
  add_test(NAME console_demo
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:cafeteria_console>
            -DSCRIPT=${CMAKE_SOURCE_DIR}/tests/console_script.txt
            -DEXPECT=${CMAKE_SOURCE_DIR}/tests/console_expected.txt
            -P ${CMAKE_SOURCE_DIR}/tools/run_console_script.cmake)
endif()

if(TARGET cafeteria_admin)
  add_test(NAME admin_tool
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:cafeteria_admin>
            -DWORK=${CMAKE_BINARY_DIR}/admin_test
            -P ${CMAKE_SOURCE_DIR}/tools/run_admin_test.cmake)
endif()

add_executable(thread_exit_stress tests/stress/thread_exit_stress.cpp)
target_include_directories(thread_exit_stress PRIVATE ${HTTPLIB_DIR})
if(WIN32)
  target_link_libraries(thread_exit_stress PRIVATE ws2_32 crypt32)
endif()
add_test(NAME thread_exit_stress COMMAND thread_exit_stress)
set_tests_properties(thread_exit_stress PROPERTIES TIMEOUT 120)

# WCAG contrast of every colour pair the pages use (tools/contrast_check.cpp).
add_executable(contrast_check tools/contrast_check.cpp)
add_test(NAME contrast_check COMMAND contrast_check)

# Colours are hex only in the pages, scripts and drawings (tools/check_hex_colors.cpp).
add_executable(check_hex_colors tools/check_hex_colors.cpp)
add_test(NAME hex_colors_only COMMAND check_hex_colors ${CMAKE_SOURCE_DIR}/web)

# Static rules for every page and the stylesheet (tools/check_pages.cpp).
add_executable(check_pages tools/check_pages.cpp)
add_test(NAME page_rules COMMAND check_pages ${CMAKE_SOURCE_DIR}/web)

file(GLOB E2E_SOURCES CONFIGURE_DEPENDS tests/e2e/*.cpp)
if(E2E_SOURCES AND TARGET cafeteria_server)
  add_executable(e2e_section12 ${E2E_SOURCES})
  target_include_directories(e2e_section12 PRIVATE ${HTTPLIB_DIR})
  target_link_libraries(e2e_section12 PRIVATE cafe_core)
  add_dependencies(e2e_section12 cafeteria_server)
  add_test(NAME e2e_section12
    COMMAND e2e_section12 $<TARGET_FILE:cafeteria_server> ${CMAKE_SOURCE_DIR})
set_tests_properties(e2e_section12 PROPERTIES TIMEOUT 180)
endif()
