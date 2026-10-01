#!/bin/bash
# NameError conflict matrix — 4 name occupiers × 4 declarations
#   var, def, as (link namespace), set (no-var assignment that creates)
# Cross-kind → NameError. Same-kind declaration → NameError.
# Only non-conflict: set→var or var→set (plain reassignment), set→set.
set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
cd "$SCRIPT_DIR"  # ensure CWD for env path resolution
ALEXIS="$SCRIPT_DIR/../../../../bin/Scpt"
TEST_DIR="$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo "=== NameError Conflict Matrix (4×4) ==="
echo "  var, def, as, set (no-var assign that creates)"
echo ""

total=0; pass=0; bugs=0

# Each entry: file, prior, new, expect_error (1=must throw, 0=OK no error)
declare -a tests=(
    "01_var_var|var|var|1"
    "04_def_var|var|def|1"
    "07_as_var|var|as|1"
    "A1_var_set|var|set|0"

    "02_var_def|def|var|1"
    "05_def_def|def|def|1"
    "08_as_def|def|as|1"
    "A2_def_set|def|set|1"

    "03_var_as|as|var|1"
    "06_def_as|as|def|1"
    "09_as_as|as|as|1"
    "A3_as_set|as|set|1"

    "A4_set_var|set|var|1"
    "A5_set_def|set|def|1"
    "A6_set_as|set|as|1"
    "A7_set_set|set|set|0"
)

printf "  %-8s %-6s %-6s %-6s %-6s\n" "" "var" "def" "as" "set"
echo  "  ─────────────────────────────────────"

declare -A results
row_order=("var" "def" "as" "set")
col_order=("var" "def" "as" "set")

for entry in "${tests[@]}"; do
    IFS='|' read -r file prior new expect <<< "$entry"
    total=$((total + 1))

    output=$("$ALEXIS" "$file.axc" 2>&1) || true
    got_error=0; err_type=""
    if echo "$output" | grep -qE "(NameError|LinkError|ImportError)"; then
        got_error=1
        err_type=$(echo "$output" | grep -oE '(NameError|LinkError|ImportError)' | head -1)
    fi

    key="${prior}_${new}"
    if [ "$got_error" -eq "$expect" ]; then
        if [ "$got_error" -eq 1 ]; then
            results["$key"]="${GREEN}✓${NC}"
            pass=$((pass + 1))
        else
            results["$key"]="${GREEN}─${NC}"
            pass=$((pass + 1))
        fi
    else
        if [ "$expect" -eq 1 ]; then
            results["$key"]="${RED}✗${NC}"
            bugs=$((bugs + 1))
        else
            results["$key"]="${YELLOW}?${NC}"
            bugs=$((bugs + 1))
        fi
    fi
done

# Print matrix
for prior in "${row_order[@]}"; do
    printf "  ${CYAN}%-8s${NC}" "$prior"
    for new in "${col_order[@]}"; do
        key="${prior}_${new}"
        printf "%-10s" "${results[$key]:-  -}"
    done
    echo ""
done

echo ""
echo -e "  ${GREEN}✓${NC}=throws  ${GREEN}─${NC}=OK(silent)  ${RED}✗${NC}=BUG(missing check)"
echo ""

# Detail rows
echo "--- Details ---"
for entry in "${tests[@]}"; do
    IFS='|' read -r file prior new expect <<< "$entry"
    key="${prior}_${new}"
    output=$("$ALEXIS" "$file.axc" 2>&1) || true

    got_error=0; err_type=""; err_msg=""
    if echo "$output" | grep -qE "(NameError|LinkError|ImportError)"; then
        got_error=1
        err_type=$(echo "$output" | grep -oE '(NameError|LinkError|ImportError)' | head -1)
        err_msg=$(echo "$output" | head -1)
    fi

    if [ "$got_error" -ne "$expect" ] && [ "$expect" -eq 1 ]; then
        echo -e "  ${RED}BUG${NC} ${prior}→${new} ($file) — should throw, got: ${err_msg:-silent}"
    elif [ "$got_error" -ne "$expect" ] && [ "$expect" -eq 0 ]; then
        echo -e "  ${YELLOW}??${NC}  ${prior}→${new} ($file) — should be silent, got: ${err_msg}"
    fi
done

echo ""
echo -e "${CYAN}=== $total tests: $pass pass, $bugs BUGS ===${NC}"
echo "  Result column: 'new' declaration covering existing 'prior' name"
[ $bugs -gt 0 ] && exit 1
