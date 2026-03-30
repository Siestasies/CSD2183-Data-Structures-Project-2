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

    # capture all stdout (polygon data + timing) into output file
    "$SIMPLIFY" "$csv" "$target" > "$OUTPUT_DIR/${filename%.csv}_output.txt"
    stdout_out=$(cat "$OUTPUT_DIR/${filename%.csv}_output.txt")

    parse_ms=$(echo    "$stdout_out" | grep "CSV parsing"    | awk '{print $4}')
    setup_ms=$(echo    "$stdout_out" | grep "Data setup"     | awk '{print $4}')
    simplify_ms=$(echo "$stdout_out" | grep "Simplification" | awk '{print $4}')
    memory_kb=$(echo   "$stdout_out" | grep "Peak memory"    | awk '{print $3}')

    printf "%-45s %10d %12s %12s %12s %12s\n" \
        "$filename" "$n_verts" "$parse_ms" "$setup_ms" "$simplify_ms" "$memory_kb"
done
