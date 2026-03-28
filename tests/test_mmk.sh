#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"
SIGN_KEY="mmk-test-sign-key"
export AJA_SIGN_KEY="$SIGN_KEY"

if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN"
    exit 1
fi

TMP=$(mktemp -d /tmp/aja-mmk-test-XXXXXX)
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

mkdir -p "$TMP/libs"
cat >"$TMP/libs/dep.aja" <<'EOF'
fuc hello(name: str) -> str:
    return "hello " + name

export (
    hello
)
EOF

cat >"$TMP/libs/regdep.aja" <<'EOF'
fuc ping() -> str:
    return "pong"

export (
    ping
)
EOF

cd "$TMP"

"$BIN" mmk init >/dev/null
set +e
"$BIN" mmk add ./libs/dep.aja >/dev/null 2>"$TMP/add_no_version.err"
code=$?
set -e
if [ "$code" -eq 0 ]; then
    echo "[FAIL] mmk add should require --version"
    exit 1
fi
if ! grep -F -q -- "--version is required" "$TMP/add_no_version.err"; then
    echo "[FAIL] mmk add missing-version error message mismatch"
    exit 1
fi

"$BIN" mmk add ./libs/dep.aja --version 1.2.3 >/dev/null

if ! grep -F -q "dep==1.2.3 @ ./libs/dep.aja" requirements.txt; then
    echo "[FAIL] mmk add did not write pinned dependency"
    exit 1
fi

"$BIN" mmk pack ./libs/regdep.aja --version 2.0.0 >/dev/null
if [ ! -f .aja/pkgs/regdep-2.0.0.ajapkg ]; then
    echo "[FAIL] mmk pack did not create package file"
    exit 1
fi
"$BIN" mmk publish ./.aja/pkgs/regdep-2.0.0.ajapkg >/dev/null
"$BIN" mmk pack ./libs/regdep.aja --version 2.1.0 >/dev/null
"$BIN" mmk publish ./.aja/pkgs/regdep-2.1.0.ajapkg >/dev/null
"$BIN" mmk pack ./libs/regdep.aja --version 3.0.0 >/dev/null
"$BIN" mmk publish ./.aja/pkgs/regdep-3.0.0.ajapkg >/dev/null

"$BIN" mmk add regdep --version ^2.0 >/dev/null
if ! grep -F -q "regdep==2.1.0 @ registry://regdep/2.1.0" requirements.txt; then
    echo "[FAIL] mmk add from registry did not resolve selector to latest matching version"
    exit 1
fi

"$BIN" mmk search regdep >"$TMP/search.out"
if ! grep -F -q "regdep (latest 3.0.0)" "$TMP/search.out"; then
    echo "[FAIL] mmk search did not report regdep latest version"
    exit 1
fi

"$BIN" mmk info regdep --version ^2.0 >"$TMP/info.out"
if ! grep -F -q "selected: 2.1.0 (selector: ^2.0)" "$TMP/info.out"; then
    echo "[FAIL] mmk info did not resolve selected version from selector"
    exit 1
fi
if ! grep -F -q "latest: 3.0.0" "$TMP/info.out"; then
    echo "[FAIL] mmk info did not report latest version"
    exit 1
fi

"$BIN" mmk install >/dev/null

if ! grep -E -q '^dep==1\.2\.3 @ ./libs/dep\.aja \| hash=[0-9a-f]{64}$' requirements.lock; then
    echo "[FAIL] mmk install did not write pinned+hashed lock line"
    exit 1
fi
if ! grep -E -q '^regdep==2\.1\.0 @ registry://regdep/2\.1\.0 \| hash=[0-9a-f]{64}$' requirements.lock; then
    echo "[FAIL] mmk install did not lock registry dependency hash"
    exit 1
fi

if [ ! -f .aja/site-packages/dep.aja ]; then
    echo "[FAIL] installed module file missing"
    exit 1
fi
if [ ! -f .aja/site-packages/dep.meta ]; then
    echo "[FAIL] metadata file missing"
    exit 1
fi
if ! grep -F -q 'name=dep' .aja/site-packages/dep.meta; then
    echo "[FAIL] metadata missing module name"
    exit 1
fi
if ! grep -F -q 'version=1.2.3' .aja/site-packages/dep.meta; then
    echo "[FAIL] metadata missing version"
    exit 1
fi
if ! grep -F -q 'exports=hello' .aja/site-packages/dep.meta; then
    echo "[FAIL] metadata missing exports"
    exit 1
fi
if [ ! -f .aja/site-packages/regdep.aja ]; then
    echo "[FAIL] registry-installed module file missing"
    exit 1
fi

"$BIN" mmk verify >/dev/null

"$BIN" mmk install --locked >/dev/null

HOME_TMP="$TMP/home"
mkdir -p "$HOME_TMP"
HOME="$HOME_TMP" "$BIN" mmk install-stdlib --global --from "$ROOT/libs" >/dev/null
if [ ! -f "$HOME_TMP/.aja/site-packages/re.aja" ]; then
    echo "[FAIL] mmk install-stdlib --global did not install core module re.aja"
    exit 1
fi
if [ -f "$HOME_TMP/.aja/site-packages/query.aja" ]; then
    echo "[FAIL] mmk install-stdlib --global should not install optional module query.aja"
    exit 1
fi

HOME="$HOME_TMP" "$BIN" mmk install query --global --from "$ROOT/libs" >/dev/null
if [ ! -f "$HOME_TMP/.aja/site-packages/query.aja" ]; then
    echo "[FAIL] mmk install query --global did not install query.aja"
    exit 1
fi

cat > "$TMP/global_stdlib_import.aja" <<'EOF'
import (
    "query"
)

print(query.withParam("https://x", "a", "1"))
EOF
HOME="$HOME_TMP" "$BIN" "$TMP/global_stdlib_import.aja" >"$TMP/global_stdlib_import.out" 2>"$TMP/global_stdlib_import.err"
if ! grep -F -q "https://x?a=1" "$TMP/global_stdlib_import.out"; then
    echo "[FAIL] global stdlib import did not produce expected output"
    exit 1
fi
if [ -s "$TMP/global_stdlib_import.err" ]; then
    echo "[FAIL] global stdlib import produced stderr"
    cat "$TMP/global_stdlib_import.err"
    exit 1
fi

HOME="$HOME_TMP" "$BIN" mmk install httpx --project --from "$ROOT/libs" >/dev/null
if [ ! -f "$TMP/.aja/site-packages/httpx.aja" ]; then
    echo "[FAIL] mmk install httpx --project did not install httpx.aja"
    exit 1
fi

echo "# mutate" >> "$TMP/libs/dep.aja"
set +e
"$BIN" mmk install --locked >/dev/null 2>"$TMP/locked.err"
code=$?
set -e
if [ "$code" -eq 0 ]; then
    echo "[FAIL] mmk install --locked should fail on source hash change"
    exit 1
fi
if ! grep -F -q "hash mismatch for dep" "$TMP/locked.err"; then
    echo "[FAIL] mmk install --locked did not report hash mismatch"
    exit 1
fi

"$BIN" mmk install >/dev/null
"$BIN" mmk verify >/dev/null

AJA_SIGN_KEY= AJA_REQUIRE_SIGNATURE=1 "$BIN" mmk verify >/dev/null 2>"$TMP/verify_sig.err" || true
if ! grep -F -q "AJA_SIGN_KEY is not set" "$TMP/verify_sig.err"; then
    echo "[FAIL] mmk verify should fail when signature is required and key is missing"
    exit 1
fi

echo "# mutate installed artifact" >> .aja/site-packages/regdep.aja
set +e
"$BIN" mmk verify >/dev/null 2>"$TMP/verify.err"
code=$?
set -e
if [ "$code" -eq 0 ]; then
    echo "[FAIL] mmk verify should fail on installed hash mismatch"
    exit 1
fi
if ! grep -F -q "installed file hash mismatch for regdep" "$TMP/verify.err"; then
    echo "[FAIL] mmk verify mismatch error message mismatch"
    exit 1
fi

echo "[OK]   mmk_v2"
