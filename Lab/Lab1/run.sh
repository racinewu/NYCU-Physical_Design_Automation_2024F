#!/bin/bash

TESTCASE_DIR="testcase"
INPUT_DIR="$TESTCASE_DIR/input"
OUTPUT_DIR="$TESTCASE_DIR/output"
GOLDEN_DIR="$TESTCASE_DIR/golden"
TARGET="./bin/CStitch"

# Colors
BLUE="\e[34m"
PURPLE="\e[35m"
GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[33m"
RESET="\e[0m"

# Logging functions
log_info()    { printf "[${BLUE}INFO${RESET}] %s\n" "$*"; }
log_error()   { printf "[${RED}ERROR${RESET}] %s\n" "$*"; }
log_pass()    { printf "[%bPASS%b]\n" "$GREEN" "$RESET"; }
log_fail()    { printf "[${YELLOW}FAIL${RESET}] %s\n" "$*"; }
log_missing() { printf "[${PURPLE}MISSING${RESET} %s] \n" "$1"; }

usage() {
    printf "Usage: ./run.sh <case|all> [check|clean|valgrind]\n"
    exit 1
}

run_case() {
    local CASE=$1
    local MODE=$2
    local INPUT_FILE="$INPUT_DIR/${CASE}.txt"
    local OUTPUT_FILE="$OUTPUT_DIR/${CASE}.txt"

    if [[ ! -f "$INPUT_FILE" ]]; then
        printf "\n"
        log_error "$CASE.txt: No such file"
        return
    fi
    mkdir -p "$OUTPUT_DIR"
    printf "\n"
    log_info "Running case:  $CASE..."
    if [[ "$MODE" == "valgrind" ]]; then
        valgrind --leak-check=full --show-leak-kinds=all "$TARGET" "$INPUT_FILE" "$OUTPUT_FILE"
    else
        TIME_FILE=$(mktemp)
        timeout 60s /usr/bin/time -f "Real: %e s, User: %U s, Sys: %S s, CPU%%: %P, MaxMem: %M KB, VolCS: %c" -o "$TIME_FILE" "$TARGET" "$INPUT_FILE" "$OUTPUT_FILE"
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            log_fail "Timeout for 60s\n"
        else
            printf "\n"
            cat "$TIME_FILE"
        fi
        rm -f "$TIME_FILE"
    fi
    log_info "Finished case: $CASE."
}

check_case() {
    local CASE=$1
    local OUT_FILE="$OUTPUT_DIR/${CASE}.txt"
    local GOLD_NUM="${CASE#case}"
    local GOLD_FILE="$GOLDEN_DIR/output${GOLD_NUM}.txt"

    printf "\n"
    log_info "Checking case: $CASE..."

    if [[ ! -f "$OUT_FILE" ]]; then
        log_missing "OUTPUT"
        return
    fi

    if [[ ! -f "$GOLD_FILE" ]]; then
        log_missing "GOLDEN"
        return
    fi

    if diff -wb "$OUT_FILE" "$GOLD_FILE" > /dev/null; then
        log_pass
    else
        log_fail
        diff -wb "$OUT_FILE" "$GOLD_FILE"
    fi

    log_info "Finished checking case: $CASE."
}

clean_case() {
    local CASE=$1
    printf "\n"

    if [[ "$CASE" == "all" ]]; then
        log_info "Cleaning all .txt files..."
        rm -f "$OUTPUT_DIR"/*.txt
        log_info "All .txt files cleaned."
    else
        infile="$INPUT_DIR/${CASE}.txt"
        if [[ ! -f "$infile" ]]; then
            log_error "$CASE.txt: No such case"
            return
        fi

        outfile="$OUTPUT_DIR/${CASE}.txt"
        if [[ -f "$outfile" ]]; then
            log_info "Cleaning case: $CASE..."
            rm -f "$outfile"
            log_info "Finished cleaning $CASE."
        else
            log_info "$CASE: nothing to clean."
        fi
    fi
}

# Check target existence unless cleaning
if [[ "$2" != "clean" && ! -x "$TARGET" ]]; then
    printf "\n"
    log_error "$TARGET not found or not executable!"
    printf "Please build it first (e.g. make).\n"
    exit 1
fi

# Parameter check
if [[ $# -lt 1 || $# -gt 2 ]]; then
    usage
fi

CASE=$1
MODE=$2

if [[ "$MODE" == "clean" ]]; then
    clean_case "$CASE"
    exit 0
fi

if [[ "$CASE" == "all" ]]; then
    shopt -s nullglob
    CASES=("$INPUT_DIR"/*.txt)
    shopt -u nullglob

    if [[ ${#CASES[@]} -eq 0 ]]; then
        log_error "No .txt files found in $INPUT_DIR"
        exit 1
    fi

    for input_file in $(printf "%s\n" "${CASES[@]}" | sort -V); do
        casename=$(basename "$input_file" .txt)
        if [[ "$MODE" == "check" ]]; then
            check_case "$casename"
        else
            run_case "$casename" "$MODE"
        fi
    done
    printf "\n"
    log_info "Finished all cases."
else
    if [[ "$MODE" == "check" ]]; then
        check_case "$CASE"
    else
        run_case "$CASE" "$MODE"
    fi
fi