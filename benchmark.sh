#!/bin/bash
 
SIMPLIFY=./simplify
DIR=${1:-test_cases}
OUTPUT_DIR=benchmark_outputs
CSV_OUT=benchmark_results.csv
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
 
# write CSV header
echo "file,vertices,parse_ms,setup_ms,simplify_ms,total_ms,memory_kb" > "$CSV_OUT"
 
# print console header
printf "%-45s %10s %12s %12s %12s %12s %12s\n" \
    "File" "Vertices" "Parse(ms)" "Setup(ms)" "Simplify(ms)" "Total(ms)" "Memory(kB)"
printf '%s\n' "$(printf '%.0s-' {1..125})"
 
for csv in "$DIR"/*.csv; do
    filename=$(basename "$csv")
    target=$(get_target "$filename")
    n_verts=$(( $(wc -l < "$csv") - 1 ))
 
    # stdout -> output file, stderr -> variable
    tmpfile=$(mktemp)
    "$SIMPLIFY" "$csv" "$target" > "$OUTPUT_DIR/${filename%.csv}_output.txt" 2>"$tmpfile"
    stderr_out=$(cat "$tmpfile")
    rm "$tmpfile"
 
    # stderr format:
    # Time - CSV parsing:    123.456 ms  -> $5 (4 words before value)
    # Time - Data setup:     123.456 ms  -> $5 (4 words before value)
    # Time - Simplification: 123.456 ms  -> $4 (3 words before value)
    # Time - Total:          123.456 ms  -> $4 (3 words before value)
    # Peak memory:           123456 kB   -> $3 (2 words before value)
    parse_ms=$(echo    "$stderr_out" | grep "CSV parsing"    | awk '{print $5}')
    setup_ms=$(echo    "$stderr_out" | grep "Data setup"     | awk '{print $5}')
    simplify_ms=$(echo "$stderr_out" | grep "Simplification" | awk '{print $4}')
    total_ms=$(echo    "$stderr_out" | grep "Total"          | awk '{print $4}')
    memory_kb=$(echo   "$stderr_out" | grep "Peak memory"    | awk '{print $3}')
 
    # print to console
    printf "%-45s %10d %12s %12s %12s %12s %12s\n" \
        "$filename" "$n_verts" "$parse_ms" "$setup_ms" "$simplify_ms" "$total_ms" "$memory_kb"
 
    # append to CSV
    echo "$filename,$n_verts,$parse_ms,$setup_ms,$simplify_ms,$total_ms,$memory_kb" >> "$CSV_OUT"
done
 
echo ""
echo "Results saved to $CSV_OUT"