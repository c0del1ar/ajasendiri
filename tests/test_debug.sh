#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"

if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN"
    exit 1
fi

TMP=$(mktemp -d /tmp/aja-debug-test-XXXXXX)
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

cat >"$TMP/prog.aja" <<'EOF'
a = 1
print(a)
EOF

printf 'c\n' | "$BIN" debug "$TMP/prog.aja" --break 2 >"$TMP/break.out" 2>"$TMP/break.err"
if [ "$(cat "$TMP/break.out")" != "1" ]; then
    echo "[FAIL] debug --break did not run program correctly"
    exit 1
fi
if ! grep -F -q "[debug] break at line 2" "$TMP/break.err"; then
    echo "[FAIL] debug --break did not stop at expected line"
    exit 1
fi

printf 's\np a\nc\n' | "$BIN" debug "$TMP/prog.aja" >"$TMP/step.out" 2>"$TMP/step.err"
if [ "$(cat "$TMP/step.out")" != "1" ]; then
    echo "[FAIL] debug --step mode did not run program correctly"
    exit 1
fi
if ! grep -F -q "[debug] break at line 1" "$TMP/step.err"; then
    echo "[FAIL] debug default step mode did not break at line 1"
    exit 1
fi
if ! grep -F -q "[debug] break at line 2" "$TMP/step.err"; then
    echo "[FAIL] debug step did not reach line 2"
    exit 1
fi
if ! grep -F -q "1 (int)" "$TMP/step.err"; then
    echo "[FAIL] debug print variable command failed"
    exit 1
fi

echo "[OK]   debug_v1"
