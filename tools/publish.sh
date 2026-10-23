#!/bin/sh
# Publishes the project folder to GitHub without its private history: the files
# of the current "main" branch become one new commit on the "public" branch,
# whose history started clean, and that branch is pushed to GitHub's main.
# Left out: docs/proposal/ (the original
# proposal document). Pushing GitHub's main runs the online-demo workflow.
#
#   sh tools/publish.sh "What changed"      publish
#   DRY_RUN=1 sh tools/publish.sh            show what would change, push nothing
set -eu
msg=${1:-"Update from the project folder"}
cd "$(git rev-parse --show-toplevel)"

if [ -n "$(git status --porcelain)" ]; then
  echo "Commit (or stash) your changes on main first." >&2
  exit 1
fi

# Take in anything committed on GitHub itself (for example an edit in the browser).
git fetch -q origin
if git merge-base --is-ancestor public origin/main; then
  git update-ref refs/heads/public origin/main
elif ! git merge-base --is-ancestor origin/main public; then
  echo "GitHub's main and the local public branch have both changed; sort that out first." >&2
  exit 1
fi

# The files of main, minus the private folders, as a tree object (nothing is checked out).
idx=.git/publish-index
rm -f "$idx"
GIT_INDEX_FILE=$idx git read-tree main
GIT_INDEX_FILE=$idx git rm -r -q --cached --ignore-unmatch docs/proposal
tree=$(GIT_INDEX_FILE=$idx git write-tree)
rm -f "$idx"

if [ "$tree" = "$(git rev-parse 'public^{tree}')" ]; then
  echo "Nothing new to publish: GitHub already has these files."
  exit 0
fi
git diff --stat public "$tree" | tail -n 1
if [ "${DRY_RUN:-}" = 1 ]; then
  git diff --name-status public "$tree"
  echo "Dry run: nothing committed or pushed."
  exit 0
fi

commit=$(git commit-tree "$tree" -p public -m "$msg")
git update-ref refs/heads/public "$commit"
git push origin public:main
echo "Published $(git rev-parse --short public). The online demo rebuilds in a minute or two."
