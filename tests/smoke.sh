#!/usr/bin/env sh
# Very thin end-to-end smoke test: ensure binary prints version.

BIN=$1

[ -x "$BIN" ] || {
    echo "Binary $BIN not found"
    exit 1
}

OUT=$("$BIN" -V)
echo "$OUT" | grep -q 0.1.3 # exits 1 → failure
