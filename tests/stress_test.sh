#!/bin/bash
# Progressive complexity stress test
# Each level is harder. Stop at first failure, fix, retry.
set -o pipefail

CORTEX="$(cd "$(dirname "$0")/.." && pwd)/build/debug/cortex_test_harness"
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
PASS=0; FAIL=0; LEVEL=0

run_level() {
    local name="$1" setup="$2" prompt="$3" verify="$4" timeout="${5:-180}"
    LEVEL=$((LEVEL + 1))
    local dir="/tmp/cortex-stress-${LEVEL}"

    echo -e "\n${YELLOW}═══ LEVEL ${LEVEL}: ${name} ═══${NC}"

    rm -rf "$dir" && mkdir -p "$dir"
    [ -n "$setup" ] && (cd "$dir" && eval "$setup") 2>/dev/null

    # Run with timeout
    timeout "$timeout" "$CORTEX" "$prompt" "$dir" > "/tmp/cortex-stress-${LEVEL}.log" 2>&1
    local exit_code=$?

    if [ $exit_code -eq 124 ]; then
        echo -e "  ${RED}TIMEOUT (${timeout}s)${NC}"
        FAIL=$((FAIL + 1)); return 1
    fi

    # Verify
    local verify_out
    verify_out=$(cd "$dir" && eval "$verify" 2>&1)
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓ PASS${NC}"
        PASS=$((PASS + 1)); return 0
    else
        echo -e "  ${RED}✗ FAIL${NC}: ${verify_out:0:150}"
        FAIL=$((FAIL + 1)); return 1
    fi
}

echo "╔═══════════════════════════════════════╗"
echo "║  JerryCode Progressive Stress Test   ║"
echo "╚═══════════════════════════════════════╝"

# Level 1: Trivial — single function
run_level "Single function" \
    'echo "#include <iostream>
int main() { return 0; }" > main.cpp' \
    "Add a function max3(int a, int b, int c) that returns the largest of three integers. Call it from main with (3,7,5) and print the result. Compile with g++ -o test main.cpp and run." \
    'g++ -o test main.cpp && test "$(./test | tr -d "[:space:]")" = "7"' \
    60

# Level 2: Two files
run_level "Two files" \
    'echo "#include <iostream>
int main() { return 0; }" > main.cpp' \
    'Create math_utils.hpp with functions: int gcd(int a, int b) using Euclidean algorithm, and int lcm(int a, int b). Update main.cpp to print gcd(12,8) and lcm(12,8). Compile with g++ -std=c++17 -o test main.cpp and run.' \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "4" && echo "$output" | grep -q "24"' \
    90

# Level 3: Bug fix with understanding
run_level "Bug fix" \
    'echo "#include <iostream>
#include <vector>
#include <algorithm>
std::vector<int> merge_sorted(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> result;
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] <= b[j]) result.push_back(a[i++]);
        else result.push_back(b[i++]);  // BUG: should be b[j++]
    }
    while (i < a.size()) result.push_back(a[i++]);
    while (j < b.size()) result.push_back(b[j++]);
    return result;
}
int main() {
    auto r = merge_sorted({1,3,5}, {2,4,6});
    for (auto x : r) std::cout << x << \" \";
    std::cout << std::endl;
}" > main.cpp' \
    "Fix the bug in main.cpp. The merge_sorted function has an indexing error. Compile with g++ -std=c++17 -o test main.cpp and run. It should print: 1 2 3 4 5 6" \
    'g++ -std=c++17 -o test main.cpp && test "$(./test | tr -d "\n")" = "1 2 3 4 5 6 "' \
    90

# Level 4: Three files with dependencies
run_level "Three files" \
    '' \
    'Create a C++ project: 1) vec2.hpp - a Vec2 struct with x,y doubles, operator+, operator-, operator*(scalar), and a length() method. 2) circle.hpp - a Circle struct with Vec2 center and double radius, with area() and contains(Vec2 point) methods. 3) main.cpp - create a circle at (0,0) radius 5, test if points (3,4) and (4,4) are inside, print results. Compile with g++ -std=c++17 -o test main.cpp and run.' \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -qi "inside\|true\|yes" && echo "$output" | grep -qi "outside\|false\|no"' \
    120

# Level 5: Python with multiple functions
run_level "Python project" \
    '' \
    'Create a Python module text_stats.py with functions: word_count(text), char_frequency(text) returning a dict, most_common_word(text), and average_word_length(text). Create main.py that tests all functions with the text "the quick brown fox jumps over the lazy dog the fox" and prints all results. Run with python3 main.py.' \
    'output=$(python3 main.py) && echo "$output" | grep -qi "the" && echo "$output" | grep -q "[0-9]"' \
    90

# Level 6: Refactor + extend
run_level "Refactor and extend" \
    'echo "#include <iostream>
#include <string>
using namespace std;
int main() {
    string s = \"Hello World\";
    // reverse
    string r = \"\";
    for (int i = s.length()-1; i >= 0; i--) r += s[i];
    cout << r << endl;
    // uppercase
    string u = \"\";
    for (int i = 0; i < s.length(); i++) {
        if (s[i] >= 97 && s[i] <= 122) u += (char)(s[i]-32);
        else u += s[i];
    }
    cout << u << endl;
    // count vowels
    int v = 0;
    for (int i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c==65||c==69||c==73||c==79||c==85||c==97||c==101||c==105||c==111||c==117) v++;
    }
    cout << v << endl;
    return 0;
}" > main.cpp' \
    "Refactor main.cpp: extract three functions (reverse_string, to_uppercase, count_vowels) into string_utils.hpp. Use standard library functions where possible (std::reverse, std::toupper, etc). Keep main.cpp clean — just calls the functions and prints. Compile with g++ -std=c++17 -o test main.cpp and run. Output must be: dlroW olleH then HELLO WORLD then 3" \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | head -1 | grep -q "dlroW olleH" && echo "$output" | sed -n 2p | grep -q "HELLO WORLD" && echo "$output" | sed -n 3p | grep -q "3"' \
    120

# Level 7: Data structure with template
run_level "Template stack" \
    '' \
    'Create stack.hpp: a header-only template Stack<T> class backed by a std::vector with push, pop (returns and removes top), top (returns reference), empty, and size. Throw std::runtime_error on pop/top of empty stack. Create main.cpp that pushes 1-5, pops and prints each. Compile with g++ -std=c++17 -o test main.cpp and run.' \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "5" && echo "$output" | grep -q "1"' \
    120

# Level 8: Bash scripting
run_level "Bash tool" \
    'mkdir -p src && for i in 1 2 3 4 5; do echo "line $i" > "src/file${i}.txt"; done
echo "important data" > src/keep.dat' \
    'Create a bash script backup.sh that: 1) Creates a backup/ directory. 2) Copies all .txt files from src/ to backup/. 3) Prints how many files were copied. 4) Lists the backup directory contents. Run it.' \
    'test -d backup && test "$(ls backup/*.txt 2>/dev/null | wc -l)" -eq 5' \
    60

# Level 9: Multi-file with Makefile
run_level "Build system" \
    '' \
    'Create a C++ project with a Makefile: 1) logger.hpp - a simple Logger class with info(), warn(), error() methods that print timestamped messages to stdout like "[INFO 12:00:00] message". 2) app.hpp - an App class that takes a Logger reference, has init() and run() methods. 3) main.cpp - creates Logger and App, calls init then run. 4) Makefile that compiles with g++ -std=c++17. All headers are header-only. Build with make and run.' \
    'g++ -std=c++17 -o app main.cpp && ./app | grep -q "INFO"' \
    120

# Level 10: Complex — config file parser
run_level "Config parser" \
    'echo "host=localhost
port=8080
debug=true
name=MyApp
max_connections=100" > config.txt' \
    'Create config_parser.hpp: a header-only ConfigParser class that reads key=value config files. Methods: load(filename), get(key) returns string, get_int(key) returns int, get_bool(key) returns bool, has(key) returns bool. Throw std::runtime_error for missing keys. Create main.cpp that loads config.txt and prints all values. Compile with g++ -std=c++17 -o test main.cpp and run.' \
    'g++ -std=c++17 -o test main.cpp && output=$(./test) && echo "$output" | grep -q "localhost" && echo "$output" | grep -q "8080" && echo "$output" | grep -q "true\|1"' \
    120

echo ""
echo "╔═══════════════════════════════════════╗"
echo "║  Results: ${PASS} pass, ${FAIL} fail / $((PASS+FAIL)) total ║"
echo "╚═══════════════════════════════════════╝"
exit $FAIL
