#!/usr/bin/env bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <keymap.kmap>"
    echo "Format a keymap file to a standard format."
    echo "The keymap file should be in the format of a standard keymap file."
    echo "The keymap file will be backed up to <keymap.kmap>.bak"
    echo "The formatted keymap file will be saved to <keymap.kmap>.fmt"
    exit 1
fi

cp "$1" "$1.bak"

awk '
BEGIN {
    FS = "|"; OFS = "|";
}
{
    # Step 1: If line is fully blank (just spaces, tabs, or newline), skip it
    if ($0 ~ /^[ \t\r\n]*$/) {
        next;
    }

    # Step 2: If it’s a comment line, print it as-is
    if ($0 ~ /^[ \t]*\/\//) {
        print $0;
        next;
    }

    # Step 3: Make a stripped version to detect pipe-only or garbage lines
    tmp = $0;
    gsub(/[ \t|]/, "", tmp);
    if (tmp == "") {
        next;
    }

    # Step 4: If we dont have at least 2 fields, skip it
    if (NF < 2) {
        next;
    }

    # Step 5: Clean and format fields
    for (i = 1; i <= NF; i++) {
        gsub(/^[ \t]+|[ \t]+$/, "", $i);
    }

    desc = sprintf("%-24s", $1);
    seq  = sprintf("%-29s", $2);

    out = desc " | " seq " |";
    for (i = 3; i <= NF; i++) {
        out = out " " $i;
        if (i < NF) {
            out = out " |";
        }
    }

    print out;
}' "$1" > "$1.fmt"




