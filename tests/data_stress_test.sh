#!/bin/bash
# Data Processing Stress Test
# Tests the framework's ability to build data lookup, comparison, and decision tools.
# These simulate real user requests involving data analysis.
set -o pipefail

CORTEX="$(cd "$(dirname "$0")/.." && pwd)/build/debug/cortex_test_harness"
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
PASS=0; FAIL=0; LEVEL=0

run_level() {
    local name="$1" setup="$2" prompt="$3" verify="$4" timeout="${5:-180}"
    LEVEL=$((LEVEL + 1))
    local dir="/tmp/cortex-data-${LEVEL}"

    echo -e "\n${YELLOW}═══ DATA ${LEVEL}: ${name} ═══${NC}"

    rm -rf "$dir" && mkdir -p "$dir"
    [ -n "$setup" ] && (cd "$dir" && eval "$setup") 2>/dev/null

    timeout "$timeout" "$CORTEX" "$prompt" "$dir" > "/tmp/cortex-data-${LEVEL}.log" 2>&1
    local exit_code=$?

    if [ $exit_code -eq 124 ]; then
        echo -e "  ${RED}TIMEOUT (${timeout}s)${NC}"
        FAIL=$((FAIL + 1)); return 1
    fi

    local verify_out
    verify_out=$(cd "$dir" && eval "$verify" 2>&1)
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓ PASS${NC}"
        PASS=$((PASS + 1)); return 0
    else
        echo -e "  ${RED}✗ FAIL${NC}: ${verify_out:0:200}"
        FAIL=$((FAIL + 1)); return 1
    fi
}

echo "╔═══════════════════════════════════════════╗"
echo "║  JerryCode Data Processing Stress Test   ║"
echo "╚═══════════════════════════════════════════╝"

# ─── DATA 1: CSV grade checker ──────────────────────────────────
# Given student scores, determine pass/fail based on threshold
run_level "Grade checker" \
    'echo "name,math,science,english
Alice,85,92,78
Bob,45,38,52
Charlie,72,65,70
Diana,91,88,95
Eve,30,42,35
Frank,68,71,59" > students.csv' \
    'Create a C++ program grade_checker.cpp that reads students.csv, calculates each student average, and outputs PASS if average >= 60, FAIL otherwise. Print each student as: "Name: avg=XX PASS/FAIL". Also print summary: total students, pass count, fail count. Compile with g++ -std=c++17 -o checker grade_checker.cpp and run.' \
    'g++ -std=c++17 -o checker grade_checker.cpp && output=$(./checker) && echo "$output" | grep -q "Alice.*PASS" && echo "$output" | grep -q "Bob.*FAIL" && echo "$output" | grep -q "Eve.*FAIL" && echo "$output" | grep -q "Diana.*PASS"' \
    180

# ─── DATA 2: Inventory validator ────────────────────────────────
# Cross-reference two data files and find discrepancies
run_level "Inventory cross-check" \
    'echo "SKU,product,expected_qty
A001,Widget,100
A002,Gadget,50
A003,Doohickey,75
A004,Thingamajig,200
A005,Whatchamacallit,30" > expected.csv
echo "SKU,product,actual_qty
A001,Widget,98
A002,Gadget,50
A003,Doohickey,80
A005,Whatchamacallit,25
A006,Mystery,10" > actual.csv' \
    'Create inventory_check.cpp that reads expected.csv and actual.csv, cross-references by SKU, and reports: 1) MATCH if quantities are equal, 2) SHORTAGE if actual < expected, 3) SURPLUS if actual > expected, 4) MISSING if SKU in expected but not actual, 5) EXTRA if SKU in actual but not expected. Print summary counts. Compile with g++ -std=c++17 -o checker inventory_check.cpp and run.' \
    'g++ -std=c++17 -o checker inventory_check.cpp && output=$(./checker) && echo "$output" | grep -qi "A001.*SHORT" && echo "$output" | grep -qi "A002.*MATCH" && echo "$output" | grep -qi "A004.*MISS" && echo "$output" | grep -qi "A006.*EXTRA"' \
    180

# ─── DATA 3: Log analyzer with pattern matching ─────────────────
# Parse log file, categorize entries, find anomalies
run_level "Log analyzer" \
    'echo "2024-01-15 08:30:00 INFO Server started
2024-01-15 08:31:15 INFO User login: alice
2024-01-15 08:32:00 WARNING High memory usage: 85%
2024-01-15 08:33:45 INFO User login: bob
2024-01-15 08:34:00 ERROR Database connection failed
2024-01-15 08:34:05 ERROR Retry failed: timeout
2024-01-15 08:35:00 INFO Database reconnected
2024-01-15 08:36:00 WARNING CPU usage: 92%
2024-01-15 08:37:00 ERROR Disk space critical: 95%
2024-01-15 08:38:00 INFO User logout: alice
2024-01-15 08:39:00 INFO User login: charlie
2024-01-15 08:40:00 WARNING Memory usage: 88%" > server.log' \
    'Create log_analyzer.cpp that reads server.log and outputs: 1) count of INFO, WARNING, ERROR entries, 2) list all ERROR messages, 3) list all WARNING messages with their timestamps, 4) unique users who logged in, 5) whether there are consecutive errors (flag as CRITICAL). Compile with g++ -std=c++17 -o analyzer log_analyzer.cpp and run.' \
    'g++ -std=c++17 -o analyzer log_analyzer.cpp && output=$(./analyzer) && echo "$output" | grep -qi "error.*3\|3.*error" && echo "$output" | grep -qi "warning.*3\|3.*warn" && echo "$output" | grep -qi "alice" && echo "$output" | grep -qi "critical\|consecutive"' \
    180

# ─── DATA 4: JSON-like config validator ─────────────────────────
# Parse a config, validate against rules, report violations
run_level "Config validator" \
    'echo "server_port=8080
max_connections=500
timeout_seconds=30
log_level=debug
database_host=localhost
database_port=5432
ssl_enabled=false
max_retries=3
cache_size_mb=256
backup_interval_hours=24" > app.conf
echo "server_port:int:1-65535
max_connections:int:1-1000
timeout_seconds:int:1-300
log_level:enum:debug,info,warn,error
database_host:string:notempty
database_port:int:1-65535
ssl_enabled:bool
max_retries:int:0-10
cache_size_mb:int:1-1024
backup_interval_hours:int:1-168" > rules.txt' \
    'Create config_validator.cpp that reads app.conf and validates each entry against rules.txt. Rules format: key:type:constraint. Types: int (with min-max range), enum (with allowed values), string (notempty), bool. Print PASS/FAIL for each config key with reason. Print overall VALID/INVALID. Compile with g++ -std=c++17 -o validator config_validator.cpp and run.' \
    'g++ -std=c++17 -o validator config_validator.cpp && output=$(./validator) && echo "$output" | grep -qi "server_port.*pass" && echo "$output" | grep -qi "log_level.*pass" && echo "$output" | grep -qi "max_connections.*pass"' \
    180

# ─── DATA 5: Python data pipeline ───────────────────────────────
# Build a complete data transformation pipeline in Python
run_level "Python data pipeline" \
    'echo "date,product,region,units,price
2024-01-01,Widget,North,100,9.99
2024-01-01,Widget,South,150,9.99
2024-01-01,Gadget,North,75,19.99
2024-01-02,Widget,North,120,9.99
2024-01-02,Gadget,South,90,19.99
2024-01-02,Widget,South,80,9.99
2024-01-03,Widget,North,200,9.99
2024-01-03,Gadget,North,60,19.99
2024-01-03,Gadget,South,110,19.99
2024-01-03,Widget,South,95,9.99" > sales.csv' \
    'Create a Python script analyze_sales.py that reads sales.csv (no pandas, just csv module) and outputs: 1) Total revenue per product. 2) Total units per region. 3) Best selling product by units. 4) Best selling day by revenue. 5) Products that sold more than 500 total units get flag "HIGH VOLUME". Run with python3 analyze_sales.py.' \
    'output=$(python3 analyze_sales.py) && echo "$output" | grep -qi "widget" && echo "$output" | grep -qi "revenue\|total" && echo "$output" | grep -qi "north\|south" && echo "$output" | grep -qi "high volume\|HIGH"' \
    120

# ─── DATA 6: Multi-file data join and report ────────────────────
# Hardest: join multiple data files, compute metrics, generate report
run_level "Multi-file data join" \
    'echo "emp_id,name,dept_id,salary
E001,Alice,D01,75000
E002,Bob,D02,65000
E003,Charlie,D01,82000
E004,Diana,D03,71000
E005,Eve,D02,58000
E006,Frank,D01,90000
E007,Grace,D03,67000
E008,Hank,D02,72000" > employees.csv
echo "dept_id,dept_name,budget
D01,Engineering,250000
D02,Marketing,200000
D03,Sales,180000" > departments.csv
echo "emp_id,rating
E001,4.5
E002,3.2
E003,4.8
E004,3.9
E005,2.8
E006,4.2
E007,3.5
E008,4.0" > reviews.csv' \
    'Create report_generator.cpp that reads all 3 CSV files, joins them by emp_id and dept_id, and generates a report: 1) Per department: name, headcount, total salary, budget remaining, average review score. 2) Flag departments where total salary exceeds budget as OVER BUDGET. 3) Flag employees with rating < 3.0 as NEEDS IMPROVEMENT. 4) Rank departments by average review score. Compile with g++ -std=c++17 -o report report_generator.cpp and run.' \
    'g++ -std=c++17 -o report report_generator.cpp && output=$(./report) && echo "$output" | grep -qi "engineering" && echo "$output" | grep -qi "marketing" && echo "$output" | grep -qi "budget" && echo "$output" | grep -qi "eve\|2\.8\|improvement\|NEEDS\|below"' \
    240

echo ""
echo "╔═══════════════════════════════════════════╗"
echo "║  Results: ${PASS} pass, ${FAIL} fail / $((PASS+FAIL)) total  ║"
echo "╚═══════════════════════════════════════════╝"
exit $FAIL
