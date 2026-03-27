#pragma once
#include "cortex/core/types.hpp"
#include <string>

namespace cortex {

class ProjectState {
public:
    explicit ProjectState(const std::string& project_root);

    std::string data_dir() const;
    std::string db_path() const;

    void ensure_data_dir() const;
    std::vector<std::string> scan_files(int max_depth = 4) const;

private:
    std::string project_root_;
};

} // namespace cortex
