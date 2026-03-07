#!/bin/bash

# =============================================================================
# Lambda DSSE Benchmarking Script
# =============================================================================
# Benchmarks the Lambda scheme for different (Size_ID, Size_KS) configurations.
#
# Output columns (CSV):
#   Size_ID, Size_KS, DB_Size_KB, Setup_us, Avg_Update_Server_us,
#   Avg_Update_Client_us, Avg_Search_Client_us, Avg_Search_Server_us,
#   Post_Processing_us
# =============================================================================

set -e

# =================== CONFIGURATION ===================
# Define the (Size_ID, Size_KS) pairs to benchmark.
# Each entry is "Size_ID:Size_KS"
CONFIGS=(
    "10:10"
    "25:25"
    "50:50"
    "75:75"
    "100:100"
    "250:250"
    "500:500"
    "750:750"
    "1000:1000"
    "2500:2500"
    "5000:5000"
    "7500:7500"
    "10000:10000"
    "25000:25000"
    "50000:50000"
    "75000:75000"
    "100000:100000"
)

OUTPUT_CSV="benchmark_results.csv"
BINARY="main_bench"

# =================== CSV HEADER ===================
echo "Size_ID,Size_KS,Setup_us,Avg_Update_Server_us,Avg_Update_Client_us,Avg_Search_Client_us,Avg_Search_Server_us,Post_Processing_us" > "$OUTPUT_CSV"

echo "============================================================"
echo "  Lambda DSSE Benchmark"
echo "  Configurations: ${#CONFIGS[@]}"
echo "  Output: $OUTPUT_CSV"
echo "============================================================"
echo ""

# =================== BENCHMARK LOOP ===================
for config in "${CONFIGS[@]}"; do
    IFS=':' read -r SIZE_ID SIZE_KS <<< "$config"

    echo "------------------------------------------------------------"
    echo "  Benchmarking: Size_ID=$SIZE_ID, Size_KS=$SIZE_KS"
    echo "------------------------------------------------------------"

    # --- Cleanup previous databases and binary ---
    rm -f "$BINARY"
    rm -rf Server_map1 Server_map2 Server_map3 Sigma_map1

    # --- Compile with the specified sizes ---
    echo "  [1/3] Compiling..."
    g++ -O2 -std=c++17 -o "$BINARY" main_bench.cpp \
        ../../../FAST/Setup.cpp \
        ../../../FAST/Update.cpp \
        ../../../FAST/Search.cpp \
        ../src/Setup.cpp ../src/Update.cpp ../src/Search.cpp ../src/Utilities.cpp \
        -lcryptopp -lrocksdb \
        -DPARAM_SIZE_ID=$SIZE_ID -DPARAM_SIZE_KS=$SIZE_KS

    if [ $? -ne 0 ]; then
        echo "  ❌ Compilation failed for Size_ID=$SIZE_ID, Size_KS=$SIZE_KS. Skipping."
        continue
    fi
    # Ensure compilation is fully flashed to disk
    wait
    sync
    echo "  ✅ Compilation successful."

    # --- Run the benchmark ---
    echo "  [2/3] Running benchmark..."
    OUTPUT=$(./"$BINARY" 2>/dev/null)
    
    # Wait for the process to fully terminate and sync disk
    wait
    sync

    if [ $? -ne 0 ]; then
        echo "  ❌ Execution failed for Size_ID=$SIZE_ID, Size_KS=$SIZE_KS. Skipping."
        continue
    fi
    echo "  ✅ Execution completed."

    # --- Parse [BENCH] lines from the output ---
    SETUP=$(echo "$OUTPUT" | grep '\[BENCH\] Setup_us=' | sed 's/.*=//')
    AVG_UPDATE_SERVER=$(echo "$OUTPUT" | grep '\[BENCH\] Avg_Update_Server_us=' | sed 's/.*=//')
    AVG_UPDATE_CLIENT=$(echo "$OUTPUT" | grep '\[BENCH\] Avg_Update_Client_us=' | sed 's/.*=//')
    AVG_SEARCH_CLIENT=$(echo "$OUTPUT" | grep '\[BENCH\] Avg_Search_Client_us=' | sed 's/.*=//')
    AVG_SEARCH_SERVER=$(echo "$OUTPUT" | grep '\[BENCH\] Avg_Search_Server_us=' | sed 's/.*=//')
    POST_PROCESSING=$(echo "$OUTPUT" | grep '\[BENCH\] Post_Processing_us=' | sed 's/.*=//')

    # --- Append to CSV ---
    echo "$SIZE_ID,$SIZE_KS,$SETUP,$AVG_UPDATE_SERVER,$AVG_UPDATE_CLIENT,$AVG_SEARCH_CLIENT,$AVG_SEARCH_SERVER,$POST_PROCESSING" >> "$OUTPUT_CSV"

    echo "  [3/3] Results recorded."
    echo ""
    echo "  Summary for Size_ID=$SIZE_ID, Size_KS=$SIZE_KS:"
    echo "    Setup:                ${SETUP} μs"
    echo "    Avg Update Server:    ${AVG_UPDATE_SERVER} μs"
    echo "    Avg Update Client:    ${AVG_UPDATE_CLIENT} μs"
    echo "    Avg Search Client:    ${AVG_SEARCH_CLIENT} μs"
    echo "    Avg Search Server:    ${AVG_SEARCH_SERVER} μs"
    echo "    Post Processing:      ${POST_PROCESSING} μs"
    echo ""
done

# =================== FINAL CLEANUP ===================
rm -f "$BINARY"
rm -rf Server_map1 Server_map2 Server_map3 Sigma_map1

# =================== GENERATE LATEX FILE ===================
OUTPUT_TEX="benchmark_results.tex"

echo "  Generating LaTeX file: $OUTPUT_TEX ..."

cat > "$OUTPUT_TEX" << 'LATEX_HEADER'
\documentclass[a4paper]{article}

\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{booktabs}
\usepackage{geometry}
\usepackage{xcolor}
\usepackage{colortbl}
\usepackage{graphicx}

\geometry{margin=1in}

\definecolor{rowgray}{gray}{0.92}

\begin{document}

\begin{table}[htbp]
\centering
\renewcommand{\arraystretch}{1.25}
\resizebox{\textwidth}{!}{%
\begin{tabular}{ccccccc}
\toprule
\textbf{Size\_ID} &
\textbf{Setup ($\mu$s)} &
\textbf{Avg Update Server ($\mu$s)} &
\textbf{Avg Update Client ($\mu$s)} &
\textbf{Avg Search Client ($\mu$s)} &
\textbf{Avg Search Server ($\mu$s)} &
\textbf{Post Processing ($\mu$s)} \\
\midrule
LATEX_HEADER

# --- Read the CSV (skip header) and write LaTeX table rows ---
ROW_NUM=0
while IFS=',' read -r SID SKS SETUP AUPSRV AUPCLT ASCCLT ASCSRV PP; do
    ROW_NUM=$((ROW_NUM + 1))

    # Alternate row shading
    if [ $((ROW_NUM % 2)) -eq 0 ]; then
        echo "\\rowcolor{rowgray}" >> "$OUTPUT_TEX"
    fi

    echo "${SID} & ${SETUP} & ${AUPSRV} & ${AUPCLT} & ${ASCCLT} & ${ASCSRV} & ${PP} \\\\" >> "$OUTPUT_TEX"
done < <(tail -n +2 "$OUTPUT_CSV")

# Wait for process substitution to finish
wait

cat >> "$OUTPUT_TEX" << 'LATEX_FOOTER'
\bottomrule
\end{tabular}%
}
\end{table}

\end{document}
LATEX_FOOTER

echo "  ✅ LaTeX file generated: $OUTPUT_TEX"

# =================== SUMMARY ===================
echo ""
echo "============================================================"
echo "  Benchmark Complete!"
echo "  CSV results    : $OUTPUT_CSV"
echo "  LaTeX report   : $OUTPUT_TEX"
echo "  Visualization  : visual/index.html"
echo "============================================================"
echo ""
echo "Note: To view the dashboard, run a local server in the 'test' directory:"
echo "  python3 -m http.server 8000"
echo "  Then visit: http://localhost:8000/visual/"
echo ""
echo "CSV Contents:"
echo ""
column -s',' -t < "$OUTPUT_CSV"
echo ""
echo "To compile the LaTeX report:"
echo "  pdflatex $OUTPUT_TEX"
pdflatex benchmark_results.tex
wait
sync
