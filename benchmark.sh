#!/bin/bash

SIMPLIFY=./simplify
DIR=test_cases
OUTPUT_DIR=benchmark_outputs
mkdir -p "$OUTPUT_DIR"

get_target() {
    case "$1" in
        *rectangle_with_two_holes*)    echo 7  ;;
        *cushion_with_hexagonal_hole*) echo 13 ;;
        *blob_with_two_holes*)         echo 17 ;;
        *wavy_with_three_holes*)       echo 21 ;;
        *lake_with_two_islands*)       echo 17 ;;
        *)                             echo 99 ;;
    esac
}

printf "%-45s %10s %12s %12s %12s %12s\n" \
    "File" "Vertices" "Parse(ms)" "Setup(ms)" "Simplify(ms)" "Memory(kB)"
printf '%s\n' "$(printf '%.0s-' {1..115})"

for csv in "$DIR"/input_*.csv; do
    filename=$(basename "$csv")
    target=$(get_target "$filename")
    n_verts=$(( $(wc -l < "$csv") - 1 ))

    # stdout -> output file, stderr -> variable
    tmpfile=$(mktemp)
    "$SIMPLIFY" "$csv" "$target" > "$OUTPUT_DIR/${filename%.csv}_output.txt" 2>"$tmpfile"
    stderr_out=$(cat "$tmpfile")
    rm "$tmpfile"

    parse_ms=$(echo    "$stderr_out" | grep "CSV parsing"    | awk '{print $4}')
    setup_ms=$(echo    "$stderr_out" | grep "Data setup"     | awk '{print $4}')
    simplify_ms=$(echo "$stderr_out" | grep "Simplification" | awk '{print $4}')
    memory_kb=$(echo   "$stderr_out" | grep "Peak memory"    | awk '{print $3}')

    printf "%-45s %10d %12s %12s %12s %12s\n" \
        "$filename" "$n_verts" "$parse_ms" "$setup_ms" "$simplify_ms" "$memory_kb"
done
