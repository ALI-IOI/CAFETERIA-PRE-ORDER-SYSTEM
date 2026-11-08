# Builds the GitHub Pages demo site from web/ (cmake -P, run by the demo_site target).
#   -DROOT=<project> -DOUT=<site folder> -DBASE=/CAFETERIA-PRE-ORDER-SYSTEM -DJS=<cafe_demo.js>
# The pages are the real ones from web/. Two changes are made to the copies only:
#   1. links start with BASE (the site is not at the root of ali-ioi.github.io);
#      /api/ calls stay as they are: web-demo/demo.js answers them with the C++ server;
#   2. every page loads web-demo/demo.js before js/api.js.

file(REMOVE_RECURSE "${OUT}")
file(COPY "${ROOT}/web/" DESTINATION "${OUT}")
file(COPY "${ROOT}/web-demo/demo.js" "${ROOT}/web-demo/cafe-worker.js" DESTINATION "${OUT}/demo")
file(COPY "${ROOT}/web-demo/google2071cf636cb837d6.html" DESTINATION "${OUT}")  # Google Search Console ownership check
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

# Search engines: a description, canonical address and link preview for the public
# pages; pages that need a login are kept out of search results. Every page gets the icon.
file(GLOB_RECURSE html "${OUT}/*.html")
set(SITE "https://ali-ioi.github.io${BASE}")
set(ICON "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%23B42318'/%3E%3Cpath d='M6 15h20a10 10 0 0 1-20 0z' fill='%23F2B531'/%3E%3Cpath d='M12 6c-1 2 1 3 0 5M16 5c-1 2 1 3 0 5M20 6c-1 2 1 3 0 5' stroke='%23FBF1E4' stroke-width='1.6' fill='none' stroke-linecap='round'/%3E%3C/svg%3E")
set(ABOUT "Live demo of a C++ data-structures project (BAUSTK DSA I) running in the browser as WebAssembly.")
set(DESC_index.html "Pre-order freshly cooked cafeteria food at BAUSTK before the cutoff: students, teachers and university staff reserve, get an order token, and collect at the counter when the token board shows it ready. ${ABOUT}")
set(DESC_login.html "Public login of the BAUSTK cafeteria pre-order system for students, teachers and university staff, with demo accounts. ${ABOUT}")
set(DESC_register.html "Create a BAUSTK cafeteria account with a university email and ID, verified by an emailed code. ${ABOUT}")
set(DESC_board.html "The BAUSTK cafeteria token board: which order tokens are ready at the counter and which are still cooking. Token numbers only. ${ABOUT}")
set(DESC_staff/login.html "Cafeteria staff login of the BAUSTK pre-order system: mark food ready, manage the menu, stock and settings, undo actions. ${ABOUT}")
foreach(f ${html})
  file(RELATIVE_PATH rel "${OUT}" "${f}")
  file(READ "${f}" text)
  if(DEFINED DESC_${rel})
    set(url "${SITE}/${rel}")
    if(rel STREQUAL "index.html")
      set(url "${SITE}/")
    endif()
    string(REGEX MATCH "<title>([^<]*)</title>" _ "${text}")
    set(title "${CMAKE_MATCH_1}")
    set(tags "<meta name=\"description\" content=\"${DESC_${rel}}\">\n  <link rel=\"canonical\" href=\"${url}\">\n  <meta property=\"og:type\" content=\"website\">\n  <meta property=\"og:url\" content=\"${url}\">\n  <meta property=\"og:title\" content=\"${title}\">\n  <meta property=\"og:description\" content=\"${DESC_${rel}}\">\n  <meta name=\"twitter:card\" content=\"summary\">")
  else()
    set(tags "<meta name=\"robots\" content=\"noindex\">")
  endif()
  string(REPLACE "</title>" "</title>\n  ${tags}\n  <link rel=\"icon\" href=\"${ICON}\">" text "${text}")
  file(WRITE "${f}" "${text}")
endforeach()
file(WRITE "${OUT}/sitemap.xml" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">
  <url><loc>${SITE}/</loc></url>
  <url><loc>${SITE}/login.html</loc></url>
  <url><loc>${SITE}/register.html</loc></url>
  <url><loc>${SITE}/board.html</loc></url>
  <url><loc>${SITE}/staff/login.html</loc></url>
</urlset>
")

# Every page must have picked up the demo script.
file(GLOB_RECURSE html "${OUT}/*.html")
foreach(f ${html})
  if(f MATCHES "/google[0-9a-f]+\\.html$")  # the Search Console file is not a page
    continue()
  endif()
  file(READ "${f}" text)
  string(FIND "${text}" "/demo/demo.js" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "make_site: ${f} does not load demo.js")
  endif()
endforeach()
message("make_site: site written to ${OUT}")
