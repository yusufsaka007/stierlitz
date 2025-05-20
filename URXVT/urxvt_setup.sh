#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROFILES_DIR="$SCRIPT_DIR/profiles"
XRES="$HOME/.Xresources"

XRES_TEMP=$(mktemp)
cp "$XRES" "$XRES_TEMP"

if [ ! -f "$XRES" ]; then
    touch "$XRES"
    echo "! Created empty .Xresources" >> "$XRES"
    echo "Created $XRES"
fi

remove_profile_block() {
    local profile_marker="$1"
    awk -v marker="$profile_marker" '
        BEGIN {skip=0}
        {
            if ($0 ~ marker) {skip=1; next}
            if (skip && /^! Profile: /) {skip=0}
            if (!skip) print
        }
    ' "$XRES_TEMP"
}

for profile_path in "$PROFILES_DIR"/*; do
    profile_name=$(basename "$profile_path")
    profile_marker="! Profile: $profile_name"

    profile_block="$profile_marker"$'\n'"$(cat "$profile_path")"

    if grep -q "$profile_marker" "$XRES_TEMP"; then
        TMP_CLEAN=$(mktemp)
        remove_profile_block "$profile_marker" > "$TMP_CLEAN"
        echo "$profile_block" >> "$TMP_CLEAN"
        mv "$TMP_CLEAN" "$XRES_TEMP"
    else
        if [ -s "$XRES_TEMP" ] && [ "$(tail -c1 "$XRES_TEMP")" != "" ]; then
            echo "" >> "$XRES_TEMP"
        fi
        echo "$profile_block" >> "$XRES_TEMP"
    fi
done

cp "$XRES_TEMP" "$XRES"
rm "$XRES_TEMP"

# Merge updated settings
xrdb -merge "$XRES"
xrdb -load "$XRES"

echo -e "\n[✔] URXVT profiles added or updated successfully."