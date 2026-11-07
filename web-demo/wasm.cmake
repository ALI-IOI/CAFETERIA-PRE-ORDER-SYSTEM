# WebAssembly build of the server for the live demo on GitHub Pages.
# Included by CMakeLists.txt when configured with Emscripten:
#
#   emcmake cmake -S . -B build-wasm -G Ninja -DSODIUM_WASM=<libsodium built for Emscripten>
#   cmake --build build-wasm --target demo_site
#
# The site is written to build-wasm/site (see web-demo/README.md).

set(SODIUM_WASM "" CACHE PATH "libsodium built with Emscripten (has include/ and lib/libsodium.a)")
if(NOT EXISTS "${SODIUM_WASM}/lib/libsodium.a")
  message(FATAL_ERROR "Set -DSODIUM_WASM to a libsodium built with Emscripten (see web-demo/README.md)")
endif()
set(DEMO_BASE "/CAFETERIA-PRE-ORDER-SYSTEM" CACHE STRING "Path the site is served from")

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
  src/ds/*.cpp src/core/*.cpp src/storage/*.cpp src/auth/*.cpp src/email/*.cpp)
add_executable(cafe_demo ${CORE_SOURCES} src/version.cpp
  src/server/app.cpp src/server/json.cpp src/server/routes.cpp src/wasm/demo_main.cpp)
# src/wasm comes first so routes.cpp gets the socket-free stand-in for httplib.h.
target_include_directories(cafe_demo PRIVATE src/wasm src ${SODIUM_WASM}/include)
target_compile_definitions(cafe_demo PRIVATE SODIUM_STATIC)
target_compile_options(cafe_demo PRIVATE -O2)
target_link_libraries(cafe_demo PRIVATE ${SODIUM_WASM}/lib/libsodium.a)
set_target_properties(cafe_demo PROPERTIES SUFFIX ".js")
target_link_options(cafe_demo PRIVATE -O2
  -sMODULARIZE=1 -sEXPORT_NAME=createCafe -sENVIRONMENT=worker
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576
  -lidbfs.js
  "-sEXPORTED_FUNCTIONS=_cafe_boot,_cafe_request,_cafe_add_part,_cafe_tick,_cafe_now,_malloc,_free"
  "-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ccall,UTF8ToString,HEAPU8"
  "--embed-file=${CMAKE_SOURCE_DIR}/demo/config@/demo/config"
  "--embed-file=${CMAKE_SOURCE_DIR}/seed@/seed")

add_custom_target(demo_site ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_BINARY_DIR}/site -DBASE=${DEMO_BASE}
          -DJS=$<TARGET_FILE:cafe_demo> -P ${CMAKE_SOURCE_DIR}/web-demo/make_site.cmake
  DEPENDS cafe_demo
  COMMENT "Assembling the demo site in ${CMAKE_BINARY_DIR}/site")
