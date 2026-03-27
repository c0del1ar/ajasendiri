#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/ajasendiri"
PASS_DIR="$ROOT/tests/spec/pass"
FAIL_DIR="$ROOT/tests/spec/fail"
CHECK_PASS_DIR="$ROOT/tests/spec/check/pass"
CHECK_FAIL_DIR="$ROOT/tests/spec/check/fail"

TOTAL=0
PASSED=0
HARNESS_ERROR=0

run_case() {
    kind="$1"
    src="$2"
    mode="${3:-run}"

    TOTAL=$((TOTAL + 1))
    tmp_out=$(mktemp)
    tmp_err=$(mktemp)

    stdin_file="$src.stdin"

    set +e
    if [ "$mode" = "check" ]; then
        "$BIN" check "$src" >"$tmp_out" 2>"$tmp_err"
    elif [ -f "$stdin_file" ]; then
        "$BIN" "$src" <"$stdin_file" >"$tmp_out" 2>"$tmp_err"
    else
        "$BIN" "$src" >"$tmp_out" 2>"$tmp_err"
    fi
    code=$?
    set -e

    expected_code=0
    if [ "$kind" = "fail" ]; then
        expected_code=1
    fi
    if [ -f "$src.code" ]; then
        expected_code=$(cat "$src.code")
    fi

    ok=1
    if [ "$code" -ne "$expected_code" ]; then
        ok=0
        echo "[FAIL] $src: expected exit $expected_code, got $code"
    fi

    if [ "$kind" = "pass" ]; then
        if [ -f "$src.out" ]; then
            if ! cmp -s "$tmp_out" "$src.out"; then
                ok=0
                echo "[FAIL] $src: stdout mismatch"
                echo "--- expected stdout"
                cat "$src.out"
                echo "--- actual stdout"
                cat "$tmp_out"
            fi
        elif [ -s "$tmp_out" ]; then
            ok=0
            echo "[FAIL] $src: expected empty stdout"
            echo "--- actual stdout"
            cat "$tmp_out"
        fi
        if [ -s "$tmp_err" ]; then
            ok=0
            echo "[FAIL] $src: expected empty stderr"
            echo "--- actual stderr"
            cat "$tmp_err"
        fi
    else
        expected_err=$(cat "$src.err")
        if ! grep -F -q "$expected_err" "$tmp_err"; then
            ok=0
            echo "[FAIL] $src: stderr did not contain expected text"
            echo "--- expected contains"
            printf "%s\n" "$expected_err"
            echo "--- actual stderr"
            cat "$tmp_err"
        fi
        if [ -f "$src.out" ] && ! cmp -s "$tmp_out" "$src.out"; then
            ok=0
            echo "[FAIL] $src: stdout mismatch"
            echo "--- expected stdout"
            cat "$src.out"
            echo "--- actual stdout"
            cat "$tmp_out"
        fi
    fi

    rm -f "$tmp_out" "$tmp_err"

    if [ "$ok" -eq 1 ]; then
        PASSED=$((PASSED + 1))
        echo "[OK]   $src"
    fi
}

run_target_file() {
    src="$1"
    case "$src" in
        "$PASS_DIR"/*.aja)
            if [ ! -f "$src.out" ]; then
                echo "[FAIL] $src: missing expected .out file"
                HARNESS_ERROR=1
                return
            fi
            run_case pass "$src"
            return
            ;;
        "$FAIL_DIR"/*.aja)
            if [ ! -f "$src.err" ]; then
                echo "[FAIL] $src: missing expected .err file"
                HARNESS_ERROR=1
                return
            fi
            run_case fail "$src"
            return
            ;;
        "$CHECK_PASS_DIR"/*.aja)
            run_case pass "$src" check
            return
            ;;
        "$CHECK_FAIL_DIR"/*.aja)
            if [ ! -f "$src.err" ]; then
                echo "[FAIL] $src: missing expected .err file"
                HARNESS_ERROR=1
                return
            fi
            run_case fail "$src" check
            return
            ;;
    esac

    echo "[FAIL] $src: not a recognized test location"
    HARNESS_ERROR=1
}

run_target_dir() {
    dir="$1"
    found=0
    for src in "$dir"/*.aja; do
        [ -e "$src" ] || continue
        found=1
        run_target_file "$src"
    done
    if [ "$found" -eq 0 ]; then
        echo "[FAIL] $dir: no .aja tests found"
        HARNESS_ERROR=1
    fi
}

canonical_path() {
    target="$1"
    dir=$(CDPATH= cd -- "$(dirname -- "$target")" && pwd) || return 1
    base=$(basename -- "$target")
    printf "%s/%s" "$dir" "$base"
}

if [ ! -x "$BIN" ]; then
    echo "binary not found, run: make"
    exit 1
fi

if [ "$#" -gt 0 ]; then
    for target in "$@"; do
        if [ -d "$target" ]; then
            target_abs=$(canonical_path "$target") || {
                echo "[FAIL] $target: invalid directory path"
                HARNESS_ERROR=1
                continue
            }
            run_target_dir "$target_abs"
            continue
        fi
        if [ ! -f "$target" ]; then
            echo "[FAIL] $target: file or directory not found"
            HARNESS_ERROR=1
            continue
        fi
        target_abs=$(canonical_path "$target") || {
            echo "[FAIL] $target: invalid file path"
            HARNESS_ERROR=1
            continue
        }
        run_target_file "$target_abs"
    done
else
    for src in "$PASS_DIR"/*.aja; do
        [ -e "$src" ] || continue
        [ -f "$src.out" ] || continue
        run_case pass "$src"
    done

    for src in "$FAIL_DIR"/*.aja; do
        [ -e "$src" ] || continue
        [ -f "$src.err" ] || continue
        run_case fail "$src"
    done

    for src in "$CHECK_PASS_DIR"/*.aja; do
        [ -e "$src" ] || continue
        run_case pass "$src" check
    done

    for src in "$CHECK_FAIL_DIR"/*.aja; do
        [ -e "$src" ] || continue
        [ -f "$src.err" ] || continue
        run_case fail "$src" check
    done
fi

echo ""
echo "Result: $PASSED/$TOTAL passed"

if [ "$PASSED" -ne "$TOTAL" ] || [ "$HARNESS_ERROR" -ne 0 ]; then
    exit 1
fi
