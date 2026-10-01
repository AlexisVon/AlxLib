#!/bin/bash
# Alexis Script — comprehensive language coverage test suite
# Silent on pass, only outputs failures.

SCRIPT_DIR=$(dirname "$(realpath "$0")")
BIN_DIR="$SCRIPT_DIR/../../../../bin"
ALEXIS="$BIN_DIR/Scpt"
COVER_DIR="$SCRIPT_DIR"

# ── Expected-to-fail tests (non-zero exit = pass) ──
XFAIL=(
    "exceptions/31_parse_error.axc"       # intentional syntax error
    "exceptions/04_uncaught.axc"           # uncaught exception prints call stack
)

# ── Files needing special flags ──
declare -A FLAGS
FLAGS["exceptions/18_overflow_error.axc"]="--overflow-check"
FLAGS["exceptions/19_shift_error.axc"]="--overflow-check"
FLAGS["exceptions/17_div_overflow_error.axc"]="--overflow-check"
FLAGS["exceptions/26_stack_error.axc"]="--max-stack 16"
FLAGS["exceptions/27_resource_error.axc"]="--max-vecfill 3"

# Import tests: run from cover/ so "modules/xxx.axc" resolves
declare -A CWD
CWD["modules/t1_import.axc"]="$COVER_DIR"
CWD["modules/t1b_alias_conflict.axc"]="$COVER_DIR"
CWD["modules/t7b_parent_nav.axc"]="$COVER_DIR"
CWD["modules/t4_nav.axc"]="$COVER_DIR"

is_xfail() {
    for xf in "${XFAIL[@]}"; do
        [[ "$1" == "$xf" ]] && return 0
    done
    return 1
}

# ── Run all .axc files ──
failed_count=0
total_count=0

while IFS= read -r -d '' f; do
    rel="${f#./}"
    total_count=$((total_count + 1))

    flag="${FLAGS[$rel]}"
    run_dir="${CWD[$rel]:-$COVER_DIR}"

    if is_xfail "$rel"; then
        # Expected to fail: non-zero exit = pass
        output=$(cd "$run_dir" && "$ALEXIS" $flag "$COVER_DIR/$rel" 2>&1) || true
        ec=$?
        if [ $ec -eq 0 ]; then
            echo "=== XPASS (expected fail but passed): $rel ==="
            failed_count=$((failed_count + 1))
        fi
    else
        # Expected to pass: zero exit = pass
        output=$(cd "$run_dir" && "$ALEXIS" $flag "$COVER_DIR/$rel" 2>&1) || true
        ec=$?
        if [ $ec -ne 0 ]; then
            echo "=== FAIL (exit=$ec): $rel ==="
            echo "$output"
            echo ""
            failed_count=$((failed_count + 1))
        fi
    fi
done < <(find "$COVER_DIR" -name "*.axc" -not -path "*/modules/modules/*" -print0 | sort -z)

echo "=== Cover Summary ==="
echo "Total: $total_count, Failed: $failed_count"
if [ $failed_count -gt 0 ]; then
    exit 1
fi
