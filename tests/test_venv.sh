#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"

if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN"
    exit 1
fi

TMP=$(mktemp -d /tmp/aja-venv-test-XXXXXX)
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

mkdir -p "$TMP/proj"
cd "$TMP/proj"

"$BIN" venv .venv >/dev/null

if [ ! -x .venv/bin/ajasendiri ]; then
    echo "[FAIL] venv runner was not created"
    exit 1
fi
if [ ! -f .venv/bin/activate ]; then
    echo "[FAIL] venv activate script was not created"
    exit 1
fi
if [ ! -d .venv/site-packages ]; then
    echo "[FAIL] venv site-packages directory missing"
    exit 1
fi

mkdir -p libs
cat > libs/dep.aja <<'EOM'
fuc hello(name: str) -> str:
    return "hello " + name

export (
    hello
)
EOM

"$BIN" mmk init >/dev/null
"$BIN" mmk add ./libs/dep.aja --version 1.0.0 >/dev/null
./.venv/bin/ajasendiri mmk install >/dev/null

if [ ! -f .venv/site-packages/dep.aja ]; then
    echo "[FAIL] dependency was not installed into venv site-packages"
    exit 1
fi
if [ -f .aja/site-packages/dep.aja ]; then
    echo "[FAIL] dependency should not be installed into project .aja/site-packages when venv is active"
    exit 1
fi

cat > app.aja <<'EOM'
import (
    "dep"
)

print(dep.hello("venv"))
EOM

./.venv/bin/ajasendiri app.aja > "$TMP/app.out" 2> "$TMP/app.err"
if ! grep -F -q "hello venv" "$TMP/app.out"; then
    echo "[FAIL] runtime import from venv failed"
    exit 1
fi
if [ -s "$TMP/app.err" ]; then
    echo "[FAIL] runtime import from venv produced stderr"
    cat "$TMP/app.err"
    exit 1
fi

./.venv/bin/ajasendiri mmk install-stdlib --from "$ROOT/libs" >/dev/null
if [ ! -f .venv/site-packages/re.aja ]; then
    echo "[FAIL] install-stdlib should install core modules into active venv"
    exit 1
fi
if [ -f .venv/site-packages/query.aja ]; then
    echo "[FAIL] install-stdlib default should not install optional modules"
    exit 1
fi

./.venv/bin/ajasendiri mmk install query --from "$ROOT/libs" >/dev/null
if [ ! -f .venv/site-packages/query.aja ]; then
    echo "[FAIL] mmk install <module> should install bundled module into active venv"
    exit 1
fi

echo "[OK]   venv"
