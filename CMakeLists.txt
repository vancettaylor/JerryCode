cmake_minimum_required(VERSION 3.22)
project(cortex VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Dependencies via FetchContent ─────────────────────────────
include(FetchContent)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(ftxui json httplib)

# ─── SQLite3 (amalgamation, built inline) ──────────────────────
FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
)
FetchContent_MakeAvailable(sqlite3)

# Build SQLite as a static library
if(NOT TARGET sqlite3)
    add_library(sqlite3 STATIC
        ${sqlite3_SOURCE_DIR}/sqlite3.c
    )
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    if(UNIX)
        target_link_libraries(sqlite3 PRIVATE dl pthread)
    endif()
endif()

# ─── Main library (everything except main.cpp) ─────────────────
add_library(cortex_lib STATIC
    src/core/types.cpp
    src/core/context_index.cpp
    src/core/context_compiler.cpp
    src/core/context_expander.cpp
    src/core/task_manager.cpp
    src/core/prompt_engine.cpp
    src/core/template_store.cpp
    src/core/notebook.cpp
    src/core/session.cpp
    src/core/orchestrator.cpp
    src/core/timeline.cpp
    src/core/token_budget.cpp
    src/agents/agent_registry.cpp
    src/agents/file_read_agent.cpp
    src/agents/file_write_agent.cpp
    src/agents/bash_agent.cpp
    src/agents/search_agent.cpp
    src/agents/glob_agent.cpp
    src/agents/grep_agent.cpp
    src/tools/file_read.cpp
    src/tools/file_write.cpp
    src/tools/bash.cpp
    src/tools/glob.cpp
    src/tools/grep.cpp
    src/providers/anthropic.cpp
    src/providers/openai.cpp
    src/providers/sse_parser.cpp
    src/storage/metadata_store.cpp
    src/storage/project_state.cpp
    src/storage/sqlite_wrapper.cpp
    src/tui/app.cpp
    src/tui/chat_panel.cpp
    src/tui/input_area.cpp
    src/tui/status_bar.cpp
    src/tui/timeline_panel.cpp
    src/util/config.cpp
    src/util/log.cpp
    src/util/uuid.cpp
    src/util/string_util.cpp
    src/tui/theme.cpp
)

target_include_directories(cortex_lib PUBLIC include)

# OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)
target_compile_definitions(cortex_lib PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(cortex_lib PUBLIC
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    sqlite3
)

if(UNIX)
    target_link_libraries(cortex_lib PUBLIC pthread)
endif()

# Prompt and schema directories available at runtime
target_compile_definitions(cortex_lib PUBLIC
    CORTEX_PROMPTS_DIR="${CMAKE_SOURCE_DIR}/prompts"
    CORTEX_SCHEMAS_DIR="${CMAKE_SOURCE_DIR}/schemas"
)

# ─── Executable ─────────────────────────────────────────────────
add_executable(jerrycode src/main.cpp)
target_link_libraries(jerrycode PRIVATE cortex_lib)

# ─── Test harness (non-TUI, for development testing) ───────────
add_executable(cortex_test_harness src/test_harness.cpp)
target_link_libraries(cortex_test_harness PRIVATE cortex_lib)

# ─── Tests ──────────────────────────────────────────────────────
option(CORTEX_BUILD_TESTS "Build tests" ON)
if(CORTEX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
