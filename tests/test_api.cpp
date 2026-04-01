/**
 * @file test_api.cpp
 * @brief Test the JerryCode API — verifies the programmatic interface works.
 *
 * Usage: ./test_api "task description" /path/to/project
 */
#include <cortex/api.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::string task = "Add a square(int n) function that returns n*n. Call it from main to print square(7). Compile with g++ -o test main.cpp and run.";
    std::string project = "/tmp/jerrycode-api-test";

    if (argc > 1) task = argv[1];
    if (argc > 2) project = argv[2];

    // Setup test project
    std::filesystem::create_directories(project);
    {
        std::ofstream f(project + "/main.cpp");
        f << "#include <iostream>\nint main() { return 0; }\n";
    }

    std::cout << "=== JerryCode API Test ===\n";
    std::cout << "Task:    " << task.substr(0, 60) << "...\n";
    std::cout << "Project: " << project << "\n\n";

    // Run through the API
    auto result = cortex::run_task(task, project, {
        .model = "qwen3-coder-next-80b",
        .base_url = "http://192.168.11.25:8080"
    }, [](const std::string& phase, const std::string& msg) {
        if (phase == "phase") {
            std::cout << "  [" << msg << "]\n";
        } else if (phase == "error") {
            std::cerr << "  ERROR: " << msg << "\n";
        }
    });

    std::cout << "\n=== Results ===\n";
    std::cout << "Success:    " << (result.success ? "YES" : "NO") << "\n";
    std::cout << "Tasks:      " << result.tasks_completed << "/" << result.tasks_total
              << " (" << result.tasks_failed << " failed)\n";
    std::cout << "LLM Calls:  " << result.llm_calls << "\n";
    std::cout << "Tokens:     " << result.tokens_in << " in, " << result.tokens_out << " out\n";
    std::cout << "Time:       " << static_cast<int>(result.time_ms / 1000) << "s\n";
    std::cout << "Errors Fixed: " << result.errors_fixed << "\n";

    // Verify the output
    std::cout << "\n=== Verification ===\n";
    auto test_binary = project + "/test";
    if (std::filesystem::exists(test_binary)) {
        std::cout << "Binary exists: YES\n";
        // Run it
        auto output_cmd = std::string("cd ") + project + " && ./test 2>&1";
        auto pipe = popen(output_cmd.c_str(), "r");
        if (pipe) {
            char buf[256];
            std::string output;
            while (fgets(buf, sizeof(buf), pipe)) output += buf;
            pclose(pipe);
            std::cout << "Output: " << output;
            if (output.find("49") != std::string::npos) {
                std::cout << "PASS: Output contains 49 (7^2)\n";
                return 0;
            } else {
                std::cout << "FAIL: Expected 49 in output\n";
                return 1;
            }
        }
    } else {
        // Try compiling manually
        auto compile_cmd = std::string("cd ") + project + " && g++ -o test main.cpp 2>&1";
        auto pipe = popen(compile_cmd.c_str(), "r");
        if (pipe) {
            char buf[256];
            std::string output;
            while (fgets(buf, sizeof(buf), pipe)) output += buf;
            int status = pclose(pipe);
            if (status == 0) {
                std::cout << "Compiled manually: YES\n";
                auto run_cmd = std::string("cd ") + project + " && ./test 2>&1";
                auto rpipe = popen(run_cmd.c_str(), "r");
                if (rpipe) {
                    std::string rout;
                    while (fgets(buf, sizeof(buf), rpipe)) rout += buf;
                    pclose(rpipe);
                    std::cout << "Output: " << rout;
                    if (rout.find("49") != std::string::npos) {
                        std::cout << "PASS\n";
                        return 0;
                    }
                }
            } else {
                std::cout << "Compile failed: " << output << "\n";
            }
        }
        std::cout << "FAIL\n";
        return 1;
    }

    return result.success ? 0 : 1;
}
