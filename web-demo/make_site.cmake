# Builds the GitHub Pages demo site from web/ (cmake -P, run by the demo_site target).
#   -DROOT=<project> -DOUT=<site folder> -DBASE=/CAFETERIA-PRE-ORDER-SYSTEM -DJS=<cafe_demo.js>
# The pages are the real ones from web/. Two changes are made to the copies only:
#   1. links start with BASE (the site is not at the root of ali-ioi.github.io);
#      /api/ calls stay as they are: web-demo/demo.js answers them with the C++ server;
#   2. every page loads web-demo/demo.js before js/api.js.

file(REMOVE_RECURSE "${OUT}")
file(COPY "${ROOT}/web/" DESTINATION "${OUT}")
file(COPY "${ROOT}/web-demo/demo.js" "${ROOT}/web-demo/cafe-worker.js" DESTINATION "${OUT}/demo")
get_filename_component(JS_DIR "${JS}" DIRECTORY)
file(COPY "${JS}" "${JS_DIR}/cafe_demo.wasm" DESTINATION "${OUT}/demo")
file(WRITE "${OUT}/.nojekyll" "")

file(GLOB_RECURSE pages "${OUT}/*.html" "${OUT}/*.js" "${OUT}/*.css")
foreach(f ${pages})
  if(f MATCHES "/demo/")
    continue()
  endif()
  file(READ "${f}" text)
  string(REGEX REPLACE "\"/([A-Za-z])" "\"${BASE}/\\1" text "${text}")  # "/login.html" -> "BASE/login.html"
  string(REPLACE "\"${BASE}/api/" "\"/api/" text "${text}")             # API calls stay /api/
  string(REPLACE "href=\"/\"" "href=\"${BASE}/\"" text "${text}")        # home link
  string(REPLACE "wireLogout(\"logout\", \"/\")" "wireLogout(\"logout\", \"${BASE}/\")" text "${text}")
  string(REPLACE "<script src=\"${BASE}/js/api.js\"></script>"
    "<script src=\"${BASE}/demo/demo.js\"></script>\n  <script src=\"${BASE}/js/api.js\"></script>" text "${text}")
  file(WRITE "${f}" "${text}")
endforeach()

# Every page must have picked up the demo script.
file(GLOB_RECURSE html "${OUT}/*.html")
foreach(f ${html})
  file(READ "${f}" text)
  string(FIND "${text}" "/demo/demo.js" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "make_site: ${f} does not load demo.js")
  endif()
endforeach()
message("make_site: site written to ${OUT}")
