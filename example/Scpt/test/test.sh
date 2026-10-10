#!/bin/bash
# Alexis Script — full test suite (demo + cover)
# Silent on pass, only outputs failures.

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
# ALX_TEST_BIN_DIR lets the same suite run against another build tree (e.g. an ASan one), but the link
# fixtures under example/Scpt/test/bin are shared by every build tree: rebuild them from the target
# tree before running, or a non-ASan Scpt will load ASan .so files and every link test fails
BIN_DIR="${ALX_TEST_BIN_DIR:-$SCRIPT_DIR/../../../bin}"
ALEXIS="$BIN_DIR/Scpt"
DEMO_DIR="$SCRIPT_DIR/demo"
COVER_DIR="$SCRIPT_DIR/cover"

# ── helpers ──
fail() { echo "=== FAIL: $1 ==="; shift; [ $# -gt 0 ] && echo "$@"; echo ""; }

# ── Test link modules: built by the top-level CMake build (example/Scpt/CMakeLists.txt) ──
MODULE_DIR="$SCRIPT_DIR/bin"
for m in base test_store; do
    if [ ! -f "$MODULE_DIR/$m.so" ]; then
        fail "missing $MODULE_DIR/$m.so" \
             "Build first: cmake -B build -DBUILD_EXAMPLES=ON && cmake --build build -j7"
        exit 1
    fi
done

# ═══════════════════════════════════════════════
# Demo tests
# ═══════════════════════════════════════════════
failed=0
cd "$DEMO_DIR"

DEMO_FILES=(
    batch1_types_conv.axc
    batch2_containers.axc
    batch3_operators.axc
    batch4_control_flow.axc
    batch5_errors_edges.axc
    batch6_scope_module.axc
    batch7_overflow.axc
    batch8_import.axc
    batch9_link_adv.axc
    batch10_modules.axc
    batch11_name_conflict.axc
    batch12_flyweight.axc
    batch13_resource.axc
    batch14_indirect_call.axc
    batch15_coverage.axc
    batch16_edges.axc
    batch17_tco_edges.axc
    batch18_reverse_nav.axc
    batch19_gaps.axc
    batch20_link_area.axc
    batch22_bug_types.axc
    batch23_jit.axc
    batch24_excall.axc
    batch25_here.axc
)

declare -A DEMO_FLAGS
DEMO_FLAGS[batch7_overflow.axc]="--overflow-check"
DEMO_FLAGS[batch13_resource.axc]="--max-vecfill 10"

for f in "${DEMO_FILES[@]}"; do
    flags="${DEMO_FLAGS[$f]}"
    # a crash prints nothing, so silence alone is not a pass: the exit code has to be checked too
    set +e
    output=$("$ALEXIS" $flags "$f" 2>&1); ec=$?
    set -e
    if [ $ec -ne 0 ]; then
        fail "$f (exit=$ec)" "$output"; failed=$((failed + 1)); continue
    fi
    [ -n "$output" ] && { fail "$f" "$output"; failed=$((failed + 1)); }
done

# .axp round-trip
tmp_axp=$(mktemp /tmp/alexis_test_XXXXXX.axp)
if compile_out=$("$ALEXIS" -o "$tmp_axp" -c batch1_types_conv.axc 2>&1); then
    set +e
    axp_out=$("$ALEXIS" -x "$tmp_axp" 2>&1); ec=$?
    set -e
    if [ $ec -ne 0 ]; then
        fail ".axp round-trip (exit=$ec)" "$axp_out"; failed=$((failed + 1))
    elif [ -n "$axp_out" ]; then
        fail ".axp round-trip" "$axp_out"; failed=$((failed + 1))
    fi
else
    fail ".axp compile" ""; failed=$((failed + 1))
fi
rm -f "$tmp_axp"

# @ compile-time error checks
for test_case in '@a.b()' '@;'; do
    err=$("$ALEXIS" -e "$test_case" 2>&1) || true
    [ -z "$err" ] && { fail "@ compile: $test_case" ""; failed=$((failed + 1)); }
done

rm -rf __test_dir_tmp__

demo_total=${#DEMO_FILES[@]}
demo_passed=$((demo_total - failed))
echo "Demo: $demo_passed/$demo_total passed, $failed failed"

if [ $failed -gt 0 ]; then
    exit 1
fi

# ═══════════════════════════════════════════════
# Cover tests
# ═══════════════════════════════════════════════
failed=0

# Expected-to-fail: non-zero exit = pass
XFAIL=(
    # ── Tests that deliberately trigger fatal errors ──
    "exceptions/04_uncaught.axc"
    "exceptions/31_parse_error.axc"
    "exceptions/32_lex_empty_prefix.axc"
    "exceptions/33_lex_newline_in_string.axc"
    "exceptions/34_bridge_not_at_end.axc"
    "exceptions/36_float_overflow_inf.axc"
    "exceptions/37_triple_dot.axc"
    "exceptions/38_invalid_escape.axc"
    "exceptions/39_nesting_too_deep.axc"
    "exceptions/40_bridge_no_prefix.axc"
    "exceptions/26_stack_error.axc"
    "functions/test_name_collision.axc"
    "functions/test_tco_mutual.axc"
    "functions/test_tco_nontail.axc"
    "modules/t1b_alias_conflict.axc"
    "excall/error.axc"
    # ── Known engine bugs: exit 0 means bug still present ──
)

declare -A CFLAGS
CFLAGS["exceptions/18_overflow_error.axc"]="--overflow-check"
CFLAGS["exceptions/19_shift_error.axc"]="--overflow-check"
CFLAGS["exceptions/17_div_overflow_error.axc"]="--overflow-check"
CFLAGS["exceptions/26_stack_error.axc"]="--max-stack 16"
CFLAGS["exceptions/27_resource_error.axc"]="--max-vecfill 3"
CFLAGS["operators/71_arithmetic.axc"]="--overflow-check"
CFLAGS["operators/75_shift.axc"]="--overflow-check"
CFLAGS["operators/78_incdec.axc"]="--overflow-check"

declare -A CCWD
for t in modules/t1_import.axc modules/t1b_alias_conflict.axc \
         modules/t7b_parent_nav.axc modules/t4_nav.axc; do
    CCWD[$t]="$COVER_DIR"
done

is_xfail() {
    for xf in "${XFAIL[@]}"; do [[ "$1" == "$xf" ]] && return 0; done
    return 1
}

cover_total=0
while IFS= read -r -d '' f; do
    rel="${f#$COVER_DIR/}"
    flags="${CFLAGS[$rel]}"
    cwd="${CCWD[$rel]:-$COVER_DIR}"

    set +e
    (cd "$cwd" && "$ALEXIS" $flags "$f" 2>&1) > /tmp/alx_cover_out.$$; ec=$?
    output=$(tr -d '\0' < /tmp/alx_cover_out.$$)
    rm -f /tmp/alx_cover_out.$$
    set -e

    cover_total=$((cover_total + 1))
    # fail = non-zero exit OR output contains "FAIL" — cover scripts print
    # assert failures but exit 0, exit-code alone would silently pass them
    if is_xfail "$rel"; then
        if [ $ec -eq 0 ] && ! echo "$output" | grep -q "FAIL"; then
            fail "XPASS: $rel" "$output"; failed=$((failed + 1));
        fi
    else
        if [ $ec -ne 0 ] || echo "$output" | grep -q "FAIL"; then
            fail "$rel (exit=$ec)" "$output"; failed=$((failed + 1));
        fi
    fi
done < <(find "$COVER_DIR" -name "*.axc" -not -path "*/modules/modules/*" -print0 | sort -z)

cover_passed=$((cover_total - failed))
echo "Cover: $cover_passed/$cover_total passed, $failed failed"

if [ $failed -gt 0 ]; then
    echo "Cover: $failed failed"
    exit 1
fi
