#!/bin/bash
# Cortex Integration Test Suite
# Tests the dual-prompt orchestrator across diverse coding tasks.

set -o pipefail

CORTEX="$(dirname "$0")/../build/debug/cortex_test_harness"
RESULTS_DIR="/tmp/cortex-integration-results"
PASS=0
FAIL=0
ERRORS=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

run_test() {
    local name="$1"
    local setup="$2"    # bash to set up test project
    local prompt="$3"   # the task to give cortex
    local verify="$4"   # bash to verify result (exit 0 = pass)
    local model="${5:-qwen3-coder-next-80b}"

    local dir="/tmp/cortex-test-${name}"
    local logfile="${RESULTS_DIR}/${name}.log"

    echo -e "\n${YELLOW}━━━ TEST: ${name} ━━━${NC}"
    echo "  Model: ${model}"
    echo "  Prompt: ${prompt:0:80}..."

    # Setup
    rm -rf "$dir"
    mkdir -p "$dir"
    if [ -n "$setup" ]; then
        (cd "$dir" && eval "$setup") 2>/dev/null
    fi

    # Run cortex
    local start_time=$(date +%s)
    CORTEX_MODEL="$model" "$CORTEX" "$prompt" "$dir" > "$logfile" 2>&1
    local exit_code=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # Count steps
    local total_steps=$(grep -c "\[PLAN\] Step" "$logfile" 2>/dev/null || echo 0)
    local done_steps=$(grep -c "\[DONE\] SUCCESS" "$logfile" 2>/dev/null || echo 0)
    local failed_steps=$(grep -c "\[DONE\] FAILED" "$logfile" 2>/dev/null || echo 0)
    local fixes=$(grep -c "\[FIX\]" "$logfile" 2>/dev/null || echo 0)

    echo "  Steps: ${done_steps} pass, ${failed_steps} fail, ${fixes} fixes (${duration}s)"

    # Verify
    if [ -n "$verify" ]; then
        local verify_output
        verify_output=$(cd "$dir" && eval "$verify" 2>&1)
        local verify_exit=$?
        if [ $verify_exit -eq 0 ]; then
            echo -e "  ${GREEN}✓ PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}✗ FAIL${NC}"
            echo "  Verify output: ${verify_output:0:200}"
            FAIL=$((FAIL + 1))
            ERRORS="${ERRORS}\n  ${name}: ${verify_output:0:100}"
        fi
    else
        if [ $exit_code -eq 0 ] && [ $failed_steps -eq 0 ]; then
            echo -e "  ${GREEN}✓ PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}✗ FAIL${NC}"
            FAIL=$((FAIL + 1))
            ERRORS="${ERRORS}\n  ${name}: exit=${exit_code} failed_steps=${failed_steps}"
        fi
    fi
}

# ─── Setup ──────────────────────────────────────────────────────
mkdir -p "$RESULTS_DIR"
echo "╔══════════════════════════════════════════╗"
echo "║    Cortex Integration Test Suite         ║"
echo "╚══════════════════════════════════════════╝"

# ─── Test 1: Simple function addition ───────────────────────────
run_test "add-function" \
    'echo "#include <iostream>
int main() {
    std::cout << \"Hello\" << std::endl;
    return 0;
}" > main.cpp' \
    "Add a function called factorial that computes n! recursively, and call it from main to print factorial(10)" \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "3628800"'

# ─── Test 2: Create header + implementation ─────────────────────
run_test "header-impl" \
    'echo "cmake_minimum_required(VERSION 3.20)
project(calc CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(calc main.cpp)" > CMakeLists.txt
echo "#include <iostream>
int main() { return 0; }" > main.cpp' \
    "Create a calculator class in calc.hpp (header-only) with add, subtract, multiply, divide methods that work on doubles. Division by zero should throw std::invalid_argument. Update main.cpp to test all 4 operations and print results. Compile with g++ -std=c++17 -o calc main.cpp" \
    'g++ -std=c++17 -o calc main.cpp 2>&1 && ./calc 2>&1 | grep -qE "[0-9]"'

# ─── Test 3: Bug fix ───────────────────────────────────────────
run_test "bug-fix" \
    'echo "#include <iostream>
#include <vector>
#include <string>

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string result;
    for (int i = 0; i <= parts.size(); i++) {  // BUG: should be <
        if (i > 0) result += sep;
        result += parts[i];
    }
    return result;
}

int main() {
    std::vector<std::string> words = {\"hello\", \"world\", \"test\"};
    std::cout << join(words, \", \") << std::endl;
    return 0;
}" > main.cpp' \
    "There is a bug in main.cpp that causes a crash. Find and fix it. Compile and run to verify it prints: hello, world, test" \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "hello, world, test"'

# ─── Test 4: Python script ─────────────────────────────────────
run_test "python-script" \
    '' \
    "Create a Python script called fizzbuzz.py that prints FizzBuzz from 1 to 30. Define a function and CALL it at the bottom with: if __name__ == '__main__': fizzbuzz(). Run it with python3 fizzbuzz.py" \
    'output=$(python3 fizzbuzz.py) && echo "$output" | grep -q "FizzBuzz" && line_count=$(echo "$output" | wc -l) && test "$line_count" -eq 30'

# ─── Test 5: Multi-file C++ with Makefile ───────────────────────
run_test "multi-file" \
    '' \
    "Create a C++ project: point.hpp (header-only Point struct with x,y doubles and distance_to method), utils.hpp (header-only midpoint function), and main.cpp that includes both headers, creates two points, prints distance and midpoint. All headers are header-only (included by main.cpp, NOT compiled separately). Compile with: g++ -std=c++17 -o main main.cpp" \
    'g++ -std=c++17 -o main main.cpp && ./main | grep -q "[0-9]"'

# ─── Test 6: Refactor existing code ────────────────────────────
run_test "refactor" \
    'echo "#include <iostream>
#include <cmath>
using namespace std;

int main() {
    double x1=0, y1=0, x2=3, y2=4;
    double dist = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
    cout << \"Distance: \" << dist << endl;

    double x3=1, y3=1, x4=4, y4=5;
    double dist2 = sqrt((x4-x3)*(x4-x3) + (y4-y3)*(y4-y3));
    cout << \"Distance2: \" << dist2 << endl;

    double x5=0, y5=0, x6=1, y6=1;
    double dist3 = sqrt((x6-x5)*(x6-x5) + (y6-y5)*(y6-y5));
    cout << \"Distance3: \" << dist3 << endl;
    return 0;
}" > main.cpp' \
    "Refactor main.cpp to eliminate code duplication. Extract the distance calculation into a function. Keep the same output. Compile and run to verify." \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "Distance: 5" && echo "$output" | grep -q "Distance3:"'

# ─── Test 7: JSON parser (harder) ──────────────────────────────
run_test "json-parser" \
    '' \
    'Create a minimal JSON value class in json_value.hpp (header-only, no main() in it) that can hold null, bool, int, double, string types. Support construction from each type and a to_string() method. Create main.cpp that constructs several JSON values and prints them. Compile with g++ -std=c++17 -o test main.cpp and run.' \
    'g++ -std=c++17 -o test main.cpp 2>&1 && ./test 2>&1 | grep -qE "[0-9]|true|false|null|hello"'

# ─── Test 8: Shell script ──────────────────────────────────────
run_test "shell-script" \
    'mkdir -p src && echo "file1" > src/a.txt && echo "file2" > src/b.txt && echo "file3" > src/c.log' \
    "Create a bash script called organize.sh that: 1) creates directories txt_files/ and log_files/ 2) moves all .txt files from src/ into txt_files/ 3) moves all .log files from src/ into log_files/. The script must look in the src/ subdirectory specifically. Run it." \
    'test -f txt_files/a.txt && test -f txt_files/b.txt && test -f log_files/c.log'

# ─── Test 9: Error recovery (intentionally broken code) ────────
run_test "error-recovery" \
    'echo "#include <iostream>
#include <vector>

template<typename T>
T sum(const std::vector<T>& v) {
    T total;  // BUG: uninitialized
    for (const auto& x : v)
        total += x;
    return total;
}

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};
    std::cout << sum(nums) << std::endl;
    return 0;
}" > main.cpp' \
    "Fix main.cpp so it correctly prints 15 (the sum of 1..5). The variable total needs to be initialized. Compile with g++ -std=c++17 -Wall -Werror -o test main.cpp and run." \
    'g++ -std=c++17 -Wall -Werror -o test main.cpp && test "$(./test)" = "15"'

# ─── Test 10: Data structure implementation ─────────────────────
run_test "linked-list" \
    '' \
    "Create a singly linked list in linked_list.hpp as a header-only template class with: push_front, push_back, pop_front, size, empty, and an iterator (begin/end for range-for). Create main.cpp that inserts 1-5, iterates and prints each element. Compile and run." \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "1" && echo "$output" | grep -q "5"'

# ─── Summary ───────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════╗"
echo "║    Results                               ║"
echo "╚══════════════════════════════════════════╝"
echo -e "  ${GREEN}PASS: ${PASS}${NC}"
echo -e "  ${RED}FAIL: ${FAIL}${NC}"
echo "  Total: $((PASS + FAIL))"

if [ -n "$ERRORS" ]; then
    echo -e "\nFailures:${ERRORS}"
fi

echo ""
echo "Logs saved to: ${RESULTS_DIR}/"
exit $FAIL
