#include "cortex/core/context_compiler.hpp"
#include "cortex/util/string_util.hpp"
#include <fstream>
#include <sstream>

namespace cortex {

ContextCompiler::ContextCompiler(const ContextIndex& index, const TokenBudget& budget)
    : index_(index), budget_(budget) {
#ifdef CORTEX_PROMPTS_DIR
    prompts_dir_ = CORTEX_PROMPTS_DIR;
#else
    prompts_dir_ = "./prompts";
#endif
}

std::string ContextCompiler::load_template(const std::string& name) const {
    std::ifstream file(prompts_dir_ + "/" + name);
    if (!file.is_open()) return "# Template not found: " + name;
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string ContextCompiler::replace_vars(
    const std::string& tmpl,
    const std::vector<std::pair<std::string, std::string>>& vars) const {
    return render_template(tmpl, vars);
}

std::string ContextCompiler::render_file_content(const std::string& path, int max_lines) const {
    std::ifstream file(path);
    if (!file.is_open()) return "# File not found: " + path;
    std::ostringstream ss;
    std::string line;
    int count = 0;
    while (std::getline(file, line) && count < max_lines) {
        ss << ++count << "\t" << line << "\n";
    }
    return ss.str();
}

CompiledPrompt ContextCompiler::compile_phase1(const std::string& user_input) const {
    auto tmpl = load_template("phase1_plan.txt");
    auto rendered = replace_vars(tmpl, {
        {"{{context_index}}", index_.render()},
        {"{{user_input}}", user_input}
    });

    CompiledPrompt result;
    result.messages.push_back(Message::system(rendered));
    result.messages.push_back(Message::user(user_input));
    result.estimated_tokens = static_cast<int>(rendered.size()) / 4;
    return result;
}

CompiledPrompt ContextCompiler::compile_phase2(const ActionPlan& plan) const {
    auto tmpl = load_template("phase2_execute.txt");

    // Expand files requested by the plan
    std::string expanded;
    for (const auto& file : plan.files_needed) {
        expanded += "### " + file + "\n```\n";
        expanded += render_file_content(file);
        expanded += "```\n\n";
    }

    auto rendered = replace_vars(tmpl, {
        {"{{plan.task_description}}", plan.task_description},
        {"{{plan.reasoning}}", plan.reasoning},
        {"{{plan.tool_name}}", plan.tool_name},
        {"{{plan.expected_outcome}}", plan.expected_outcome},
        {"{{expanded_context}}", expanded}
    });

    CompiledPrompt result;
    result.messages.push_back(Message::system(rendered));
    result.estimated_tokens = static_cast<int>(rendered.size()) / 4;
    result.expanded_sections = plan.files_needed;
    return result;
}

CompiledPrompt ContextCompiler::compile_timeline_review(
    const std::vector<TimelineEntry>& timeline) const {
    // TODO: Implement timeline review prompt compilation
    CompiledPrompt result;
    result.messages.push_back(Message::system("Timeline review not yet implemented"));
    return result;
}

CompiledPrompt ContextCompiler::compile_agent_prompt(
    const ActionPlan& plan,
    const std::string& agent_system_prompt,
    const std::string& expanded_context) const {

    auto rendered = replace_vars(agent_system_prompt, {
        {"{{plan.task_description}}", plan.task_description},
        {"{{plan.tool_name}}", plan.tool_name},
        {"{{expanded_context}}", expanded_context}
    });

    CompiledPrompt result;
    result.messages.push_back(Message::system(rendered));
    result.estimated_tokens = static_cast<int>(rendered.size()) / 4;
    return result;
}

} // namespace cortex
