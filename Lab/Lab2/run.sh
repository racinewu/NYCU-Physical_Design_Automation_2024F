#!/bin/bash

# Config
TESTCASE_DIR="dataset"
TARGET="./bin/fp"
CHECKER="./verifier"
VISUALIZER="python3 visualizer.py"
IMAGE_DIR="images"

# Colors
BLUE="\033[34m"
PURPLE="\033[35m"
GREEN="\033[32m"
RED="\033[31m"
YELLOW="\033[33m"
RESET="\033[0m"

# Logging functions
log_info()    { printf "[${BLUE}INFO${RESET}] %s\n" "$*"; }
log_error()   { printf "\n[${RED}ERROR${RESET}] %s\n" "$*"; }
log_missing() { printf "\n[${PURPLE}MISSING${RESET}] %s\n" "$1"; }

usage() {
    cat <<USAGEEOF
Usage: $0 <alpha_value> <case|all> [check|clean|draw|valgrind]

Examples:
  $0 0.5 case1           # run a single case
  $0 0.5 all             # run all cases
  $0 0.5 case1 check     # verify a single case
  $0 0.5 all clean       # clean all outputs
  $0 0.5 case1 draw      # visualize a single case
  $0 0.5 all valgrind    # run all cases with valgrind
USAGEEOF
    exit 1
}

run_case() {
    local CASE=$1
    local IN_BLOCK="$TESTCASE_DIR/${CASE}/${CASE}.block"
    local IN_NETS="$TESTCASE_DIR/${CASE}/${CASE}.nets"
    local OUT_FILE="$TESTCASE_DIR/${CASE}/${CASE}.rpt"

    if [[ ! -f "$IN_BLOCK" ]]; then
        log_error "$IN_BLOCK: No such case"
        return
    fi
    printf "\n"
    log_info "Running case: $CASE (alpha=$ALPHA) ..."
    if [[ "$MODE" == "valgrind" ]]; then
        valgrind --leak-check=full --show-leak-kinds=all \
            "$TARGET" "$ALPHA" "$IN_BLOCK" "$IN_NETS" "$OUT_FILE"
    else
        TIME_FILE=$(mktemp)
        timeout 300s /usr/bin/time -f "Real: %e s, User: %U s, Sys: %S s, CPU%%: %P, MaxMem: %M KB, VolCS: %c" -o "$TIME_FILE" \
            "$TARGET" "$ALPHA" "$IN_BLOCK" "$IN_NETS" "$OUT_FILE"
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            log_error "Timeout for 300s"
        else
            printf "\n"
            cat "$TIME_FILE"
        fi
        rm -f "$TIME_FILE"
    fi
    log_info "Finished running case: $CASE."
}

check_case() {
    local CASE=$1
    local IN_BLOCK="$TESTCASE_DIR/${CASE}/${CASE}.block"
    local IN_NETS="$TESTCASE_DIR/${CASE}/${CASE}.nets"
    local OUT_FILE="$TESTCASE_DIR/${CASE}/${CASE}.rpt"

    if [[ ! -f "$IN_BLOCK" ]]; then
        log_error "$IN_BLOCK: No such case"
        return
    fi

    if [[ ! -f "$OUT_FILE" ]]; then
        log_missing "$CASE.rpt"
        return
    fi

    printf "\n"
    log_info "Checking case: $CASE (alpha=$ALPHA) ..."
    $CHECKER "$ALPHA" "$IN_BLOCK" "$IN_NETS" "$OUT_FILE"
    log_info "Finished checking case: $CASE."
}

clean_case() {
    local CASE=$1
    printf "\n"
    if [[ "$CASE" == "all" ]]; then
        log_info "Cleaning all .rpt and .svg files in $TESTCASE_DIR and $IMAGE_DIR ..."
        find "$TESTCASE_DIR" -name "*.rpt" -delete
        find "$IMAGE_DIR"    -name "*.svg" -delete 2>/dev/null
        log_info "Clean complete."
    else
        local RPT_FILE="$TESTCASE_DIR/${CASE}/${CASE}.rpt"
        local SVG_FILE="$IMAGE_DIR/${CASE}/${CASE}.svg"
        if [[ -f "$RPT_FILE" ]]; then
            log_info "Cleaning $RPT_FILE ..."
            rm -f "$RPT_FILE"
        else
            log_info "$RPT_FILE does not exist."
        fi
        if [[ -f "$SVG_FILE" ]]; then
            log_info "Cleaning $SVG_FILE ..."
            rm -f "$SVG_FILE"
        else
            log_info "$SVG_FILE does not exist."
        fi
        log_info "Clean complete."
    fi
}

draw_case() {
    local CASE=$1
    local IN_BLOCK="$TESTCASE_DIR/${CASE}/${CASE}.block"
    local OUT_FILE="$TESTCASE_DIR/${CASE}/${CASE}.rpt"
    local IMG_FILE="$IMAGE_DIR/${CASE}_$ALPHA.svg"

    if [[ ! -f "$IN_BLOCK" ]]; then
        log_error "$IN_BLOCK: No such case"
        return
    fi

    if [[ ! -f "$OUT_FILE" ]]; then
        log_missing "$CASE.rpt"
        return
    fi

    mkdir -p "$IMAGE_DIR"

    printf "\n"
    log_info "Drawing case: $CASE ..."
    $VISUALIZER "$IN_BLOCK" "$OUT_FILE" "$IMG_FILE"
    log_info "Saved to $IMG_FILE"
}


if [[ $# -lt 2 || $# -gt 3 ]]; then
    usage
fi

ALPHA=$1
CASE=$2
MODE=$3

# Validate alpha is numeric
if ! [[ "$ALPHA" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    log_error "alpha_value must be a number between 0 and 1 (got: '$ALPHA')"
    usage
fi

# Clean doesn't need the binary
if [[ "$MODE" == "clean" ]]; then
    clean_case "$CASE"
    exit 0
fi

# Check target existence for non-clean modes
if [[ "$MODE" != "draw" && ! -x "$TARGET" ]]; then
    log_error "$TARGET not found or not executable."
    printf "Please build it first (e.g. make).\n"
    exit 1
fi


if [[ "$CASE" == "all" ]]; then
    shopt -s nullglob
    CASES=("$TESTCASE_DIR"/*/)
    shopt -u nullglob

    if [[ ${#CASES[@]} -eq 0 ]]; then
        log_error "No cases found in $TESTCASE_DIR"
        exit 1
    fi

    for dir in $(printf "%s\n" "${CASES[@]}" | sort -V); do
        casename=$(basename "$dir")
        if [[ "$MODE" == "check" ]]; then
            check_case "$casename"
        elif [[ "$MODE" == "draw" ]]; then
            draw_case "$casename"
        else
            run_case "$casename"
        fi
    done
    printf "\n"
    log_info "Finished all cases."
else
    if [[ "$MODE" == "check" ]]; then
        check_case "$CASE"
    elif [[ "$MODE" == "draw" ]]; then
        draw_case "$CASE"
    else
        run_case "$CASE"
    fi
fi