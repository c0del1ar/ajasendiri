#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"

if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN"
    exit 1
fi

TMP=$(mktemp -d /tmp/aja-fmt-test-XXXXXX)
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

cat >"$TMP/fmt_case.aja" <<'EOF'
fuc greet(name: str, prefix: str = "Hi") -> str:
    return prefix + ", " + name

print(greet(name= "Aja",prefix ="Yo"))
EOF

set +e
"$BIN" fmt --check "$TMP/fmt_case.aja" >/dev/null 2>"$TMP/check.err"
code=$?
set -e
if [ "$code" -eq 0 ]; then
    echo "[FAIL] fmt --check should fail for unformatted named args"
    exit 1
fi
if ! grep -F -q "need formatting" "$TMP/check.err"; then
    echo "[FAIL] fmt --check did not report formatting needed"
    cat "$TMP/check.err"
    exit 1
fi

"$BIN" fmt "$TMP/fmt_case.aja" >/dev/null

if ! grep -F -q 'print(greet(name = "Aja", prefix = "Yo"))' "$TMP/fmt_case.aja"; then
    echo "[FAIL] fmt did not normalize named-call spacing"
    cat "$TMP/fmt_case.aja"
    exit 1
fi

cat >"$TMP/invalid.aja" <<'EOF'
fuc bad(a: int) -> int
    return a
EOF

set +e
"$BIN" fmt --check "$TMP/invalid.aja" >/dev/null 2>"$TMP/invalid.err"
code=$?
set -e
if [ "$code" -eq 0 ]; then
    echo "[FAIL] fmt --check should fail for invalid syntax (AST-aware validation)"
    exit 1
fi
if ! grep -F -q "parse error" "$TMP/invalid.err"; then
    echo "[FAIL] fmt did not surface parse error for invalid syntax"
    cat "$TMP/invalid.err"
    exit 1
fi

echo "[OK]   fmt_named_args"
