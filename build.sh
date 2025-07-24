#!/bin/bash
set -euo pipefail

# Enable debug if DEBUG env is set
[[ "${DEBUG:-}" == "1" ]] && set -x

# Determine package name
NAME="${1:-$(basename "$(pwd)")}"

ORIG_DIR="$(pwd)"
DEB_DIR="$ORIG_DIR/debs"
OLD_DEB_DIR="${DEB_DIR}.old_$(date '+%Y-%m-%d_%H-%M-%S')"

if [ -d "$DEB_DIR" ]; then
    mv "$DEB_DIR" "$OLD_DEB_DIR"
fi
mkdir -p "$DEB_DIR"

# Extract version from changelog
if [ ! -f debian/changelog ]; then
    echo "debian/changelog not found"
    exit 1
fi
head -n1 debian/changelog | sed -e "s/.*(\([^(]*\)).*/inline const QString VERSION {\"\1\"};/" > version.h

TMP_DIR=$(mktemp -d "/tmp/build_${NAME}_XXXXXXXX")
BUILD_DIR="$TMP_DIR/src"
mkdir -p "$BUILD_DIR"

# Generate title name (capitalize, replace mx- with MX, dashes with spaces)
NAME_MX="${NAME/#mx-/MX }"
NAME_SPACES="${NAME_MX//-/" "}"
read -ra ARRAY <<< "$NAME_SPACES"
TITLE_NAME="${ARRAY[*]^}"

# Update translations if lupdate exists
if command -v /usr/lib/qt6/bin/lupdate &>/dev/null; then
    echo "Updating translations..."
    /usr/lib/qt6/bin/lupdate ./*.pro
else
    echo "Warning: lupdate not found, skipping translation updates"
fi

# Clean before copying
make distclean || true

# Copy project to build dir, excluding unnecessary files
rsync -a --delete --exclude='.git' --exclude='*.changes' --exclude='*.dsc' \
    --exclude='*.deb' --exclude="$NAME*.tar.xz" --exclude='debs*' \
    --exclude='*.pro.user*' --exclude="build.sh" "$ORIG_DIR"/ "$BUILD_DIR"/

cd "$BUILD_DIR" || { echo "could not cd to $BUILD_DIR"; exit 1; }

# Clean again in build dir
make distclean || true

# Rename files if directories exist
for dir in . translations help; do
    if [ -d "$dir" ]; then
        rename "s/CUSTOMPROGRAMNAME/$NAME/" "$dir"/* 2>/dev/null || true
    fi
done

# Replace strings in all files
find . -type f -exec sed -i "s/CUSTOMPROGRAMNAME/$NAME/g" {} +
find . -type f -exec sed -i "s/Custom_Program_Name/$TITLE_NAME/g" {} +

# Build package
echo "Building package for $(arch) architecture..."
if [ "$(arch)" = "x86_64" ]; then
    debuild
else
    debuild -B
fi

cd "$TMP_DIR" || { echo "could not cd to $TMP_DIR"; exit 1; }

# Move build artifacts to debs dir
echo "Moving build artifacts to debs directory..."
shopt -s nullglob
artifacts=(./*.dsc ./*.deb ./*.changes ./*.tar.xz)
if [ ${#artifacts[@]} -gt 0 ]; then
    mv "${artifacts[@]}" "$DEB_DIR"
    echo "Moved ${#artifacts[@]} artifact(s) to $DEB_DIR"
else
    echo "Warning: No build artifacts found"
fi
shopt -u nullglob

cd "$ORIG_DIR" || { echo "could not cd to $ORIG_DIR"; exit 1; }

# Cleanup
rm -rf "$TMP_DIR"
[ -d "$OLD_DEB_DIR" ] && rm -rf "$OLD_DEB_DIR"
