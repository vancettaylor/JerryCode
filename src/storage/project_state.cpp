#include "cortex/storage/project_state.hpp"
#include <filesystem>

namespace cortex {

namespace fs = std::filesystem;

ProjectState::ProjectState(const std::string& project_root)
    : project_root_(project_root) {}

std::string ProjectState::data_dir() const {
    return project_root_ + "/.cortex";
}

std::string ProjectState::db_path() const {
    return data_dir() + "/metadata.db";
}

void ProjectState::ensure_data_dir() const {
    fs::create_directories(data_dir());
}

std::vector<std::string> ProjectState::scan_files(int max_depth) const {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(project_root_,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto rel = fs::relative(entry.path(), project_root_).string();
            // Skip hidden dirs and common noise
            if (rel.find("/.") != std::string::npos) continue;
            if (rel.find("/node_modules/") != std::string::npos) continue;
            if (rel.find("/build/") != std::string::npos) continue;
            if (rel.starts_with(".")) continue;
            files.push_back(rel);
            if (files.size() > 500) break;
        }
    } catch (const fs::filesystem_error&) {}
    return files;
}

} // namespace cortex
