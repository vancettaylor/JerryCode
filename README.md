# Cortex

**A dual-prompt context-managed coding agent that makes small language models dramatically more capable.**

Cortex implements a novel context management architecture where the LLM actively controls its own context window — expanding files it needs and hiding files it's done with — rather than accumulating an ever-growing flat prompt. The result: 30B-120B parameter models produce production-quality multi-file code projects that compile and run correctly.

## The Theory

Current coding agents (Claude Code, OpenCode, etc.) dump everything into a flat context and hope the model handles it. Research shows 70-90% of context tokens are wasted, and the "lost in the middle" problem means models ignore information in the middle of long prompts.

Cortex fixes this with:

1. **Task Decomposition** — User requests are broken into discrete, focused steps before execution
2. **Working Set Context** — A compact index of available files with expand/hide controls. The model sees `[+] expanded` and `[-] hidden` items and actively manages them with `@read()` and `@hide()` actions
3. **Separate Planning and Execution** — Phase 1 decomposes and plans (small token budget), Phase 2 generates code (full token budget) with only the relevant files in context
4. **Automatic Error Recovery** — Compilation errors trigger a meta-agent that analyzes the error, identifies the correct file, and generates a targeted fix
5. **Prompt Cycling** — Context management reminders rotate through the prompt to keep the model aware of its working set without repetitive instructions

## What It Can Do

With a 30B MoE model (qwen3-coder-next-80b):
- 10/10 on the integration test suite (factorial, calculator, bug fixing, Python, multi-file C++, shell scripts, refactoring, data structures)
- Built a working command-line todo app with JSON persistence (374 lines across 3 files)
- Created a thread-safe queue with producer-consumer demo that compiles and runs first try

With a 120B model (gpt-oss-120b):
- Built a working HTTP server with 3 JSON routes using raw POSIX sockets (176 lines, all routes correct)

## Building

Requirements: C++20 compiler, CMake 3.22+, OpenSSL development headers.

```bash
git clone https://github.com/vancettaylor/cortex.git
cd cortex
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j$(nproc)
```

All dependencies (FTXUI, nlohmann/json, cpp-httplib, SQLite) are fetched automatically.

## Configuration

Generate a default config:
```bash
./build/debug/cortex --init
```

This creates `cortex.json` with provider and model settings. Edit to match your setup.

Environment variables:
- `CORTEX_PROVIDER` — Provider ID
- `CORTEX_MODEL` — Model ID
- `CORTEX_BASE_URL` — API base URL
- `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` — API keys (if needed)

## Usage

```bash
# Run the test harness (non-TUI)
./build/debug/cortex_test_harness "Your coding task here" /path/to/project

# Run the integration tests
bash tests/run_integration.sh
```

## Architecture

```
User Request
  → Task Decomposition (LLM breaks request into steps, can @read files first)
  → For each task:
      → Read: load file into working set, expand in context
      → Write: LLM generates code with relevant files as reference
      → Bash: execute commands in project root
      → On error: meta-agent analyzes, fixes correct file, retries
  → Session statistics (LLM calls, tokens, context utilization)
```

## Project Structure

```
cortex/
├── include/cortex/
│   ├── core/           # Session, context expander, task manager, prompt engine
│   ├── agents/         # Tool agents (file read/write, bash, glob, grep)
│   ├── tools/          # Raw tool implementations
│   ├── providers/      # LLM providers (OpenAI-compatible, Anthropic)
│   ├── storage/        # SQLite metadata, project state
│   ├── tui/            # FTXUI terminal interface (WIP)
│   └── util/           # Config, logging, UUID, string utilities
├── src/                # Implementation files
├── prompts/            # Prompt templates
├── schemas/            # JSON schemas
└── tests/              # Integration test suite
```

## License

GPL-3.0. See [LICENSE](LICENSE).
