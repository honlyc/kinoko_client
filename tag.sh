#!/bin/bash
# Create and push a new version tag with auto-incremented version number.
#
# Usage:
#   ./tag.sh            # patch bump: v0.1.6 -> v0.1.7
#   ./tag.sh minor      # minor bump: v0.1.6 -> v0.2.0
#   ./tag.sh major      # major bump: v0.1.6 -> v1.0.0
#
set -e

cd "$(dirname "$0")"

BUMP="${1:-patch}"
if [[ "$BUMP" != "patch" && "$BUMP" != "minor" && "$BUMP" != "major" ]]; then
    echo "Usage: ./tag.sh [patch|minor|major]"
    exit 1
fi

# Refuse to tag with uncommitted changes to tracked files (untracked files are OK)
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "Error: working tree has uncommitted changes."
    git status --short --untracked-files=no
    exit 1
fi

# Sync tags and ensure HEAD is pushed
git fetch --tags --quiet
BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ -z "$(git branch -r --contains HEAD 2>/dev/null)" ]; then
    echo "Error: HEAD has not been pushed to any remote branch yet."
    exit 1
fi

# Find latest version tag
LATEST=$(git tag --list 'v*' --sort=-v:refname | head -1)
if [ -z "$LATEST" ]; then
    LATEST="v0.0.0"
fi

# Parse and bump
VERSION="${LATEST#v}"
IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"
case "$BUMP" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
esac
NEW_TAG="v${MAJOR}.${MINOR}.${PATCH}"

if git rev-parse "$NEW_TAG" >/dev/null 2>&1; then
    echo "Error: tag $NEW_TAG already exists."
    exit 1
fi

echo "Latest tag: $LATEST"
echo "New tag:    $NEW_TAG"

git tag "$NEW_TAG"
git push origin "$NEW_TAG"

echo "Done: tagged and pushed $NEW_TAG"
