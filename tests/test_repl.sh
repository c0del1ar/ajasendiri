#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"

if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN"
    exit 1
fi

TMP=$(mktemp -d /tmp/aja-repl-test-XXXXXX)
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

cat >"$TMP/repl1.in" <<'EOF'
x = 1
print(x)
.run
.exit
EOF
"$BIN" repl <"$TMP/repl1.in" >"$TMP/repl1.out" 2>"$TMP/repl1.err"
if [ "$(cat "$TMP/repl1.out")" != "1" ]; then
    echo "[FAIL] repl did not execute buffered program"
    exit 1
fi
if [ -s "$TMP/repl1.err" ]; then
    echo "[FAIL] repl emitted unexpected stderr"
    cat "$TMP/repl1.err"
    exit 1
fi

cat >"$TMP/repl2.in" <<'EOF'
x = 99
.clear
print(7)
.run
.exit
EOF
"$BIN" repl <"$TMP/repl2.in" >"$TMP/repl2.out" 2>"$TMP/repl2.err"
if [ "$(cat "$TMP/repl2.out")" != "7" ]; then
    echo "[FAIL] repl .clear did not reset buffer"
    exit 1
fi
if [ -s "$TMP/repl2.err" ]; then
    echo "[FAIL] repl emitted unexpected stderr after .clear"
    cat "$TMP/repl2.err"
    exit 1
fi

cat >"$TMP/repl3.in" <<'EOF'
a = 3
.show
.exit
EOF
"$BIN" repl <"$TMP/repl3.in" >"$TMP/repl3.out" 2>"$TMP/repl3.err"
if [ "$(cat "$TMP/repl3.out")" != "a = 3" ]; then
    echo "[FAIL] repl .show output mismatch"
    exit 1
fi
if [ -s "$TMP/repl3.err" ]; then
    echo "[FAIL] repl emitted unexpected stderr on .show"
    cat "$TMP/repl3.err"
    exit 1
fi

echo "[OK]   repl_v1"
