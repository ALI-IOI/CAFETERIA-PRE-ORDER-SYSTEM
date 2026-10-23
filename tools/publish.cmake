# Publishes the project folder to GitHub without its private history: the files
# of the current "main" branch become one new commit on the "public" branch,
# whose history started clean, and that branch is pushed to GitHub's main.
# Left out: docs/proposal/ (the original
# proposal document). Pushing GitHub's main runs the online-demo workflow.
#
#   cmake -D MESSAGE="What changed" -P tools/publish.cmake    publish
#   cmake -D DRY_RUN=1 -P tools/publish.cmake                 show what would change, push nothing

if(NOT DEFINED MESSAGE OR MESSAGE STREQUAL "")
  set(MESSAGE "Update from the project folder")
endif()

# Runs git with the given arguments; stops the script on failure. The output
# (trimmed) goes into the variable named by OUT.
function(git OUT)
  execute_process(COMMAND git ${ARGN} WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE rc
                  OUTPUT_VARIABLE text ERROR_VARIABLE err OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "git ${ARGN} failed:\n${err}")
  endif()
  set(${OUT} "${text}" PARENT_SCOPE)
endfunction()

# true when git exits with 0 (used for "is ancestor" questions)
function(git_ok OUT)
  execute_process(COMMAND git ${ARGN} WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(rc EQUAL 0)
    set(${OUT} TRUE PARENT_SCOPE)
  else()
    set(${OUT} FALSE PARENT_SCOPE)
  endif()
endfunction()

execute_process(COMMAND git rev-parse --show-toplevel OUTPUT_VARIABLE ROOT OUTPUT_STRIP_TRAILING_WHITESPACE)
git(status status --porcelain)
if(NOT status STREQUAL "")
  message(FATAL_ERROR "Commit (or stash) your changes on main first.")
endif()

# Take in anything committed on GitHub itself (for example an edit in the browser).
git(ignored fetch -q origin)
git_ok(behind merge-base --is-ancestor public origin/main)
git_ok(ahead merge-base --is-ancestor origin/main public)
if(behind)
  git(ignored update-ref refs/heads/public origin/main)
elseif(NOT ahead)
  message(FATAL_ERROR "GitHub's main and the local public branch have both changed; sort that out first.")
endif()

# The files of main, minus the private folders, as a tree object (nothing is checked out).
set(index "${ROOT}/.git/publish-index")
file(REMOVE "${index}")
set(ENV{GIT_INDEX_FILE} "${index}")
git(ignored read-tree main)
git(ignored rm -r -q --cached --ignore-unmatch docs/proposal)
git(tree write-tree)
unset(ENV{GIT_INDEX_FILE})
file(REMOVE "${index}")

git(published rev-parse "public^{tree}")
if(tree STREQUAL published)
  message(STATUS "Nothing new to publish: GitHub already has these files.")
  return()
endif()
git(summary diff --stat public ${tree})
string(REGEX REPLACE ".*\n" "" summary "${summary}")
message(STATUS "${summary}")
if(DRY_RUN)
  git(names diff --name-status public ${tree})
  message(STATUS "${names}")
  message(STATUS "Dry run: nothing committed or pushed.")
  return()
endif()

git(commit commit-tree ${tree} -p public -m "${MESSAGE}")
git(ignored update-ref refs/heads/public ${commit})
execute_process(COMMAND git push origin public:main WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "git push failed")
endif()
git(short rev-parse --short public)
message(STATUS "Published ${short}. The online demo rebuilds in a minute or two.")
