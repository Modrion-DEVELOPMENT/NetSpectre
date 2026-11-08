// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace netspro {
struct CatalogFilters {
    std::vector<int> choices = std::vector<int>(7, 0);
    const std::vector<std::string> names = {"Operating system", "Standalone", "Official source", "Latest stable", "Source", "Category", "Sort"};
    const std::vector<std::vector<std::string>> options = {
        {"All", "windows", "macos", "linux"}, {"All", "Yes", "No", "Unknown"},
        {"All", "Yes", "No"}, {"All", "Yes"}, {"All", "homebrew", "pypi", "ollama", "local"},
        {"All", "manager", "runtime", "library", "formula", "model"}, {"Catalog", "Name", "Popularity"}
    };
    void cycle(int row, int step) {
        const int count = static_cast<int>(options.at(row).size());
        choices.at(row) = (choices.at(row) + step + count) % count;
    }
    bool accepts(const nlohmann::json& meta) const {
        if (choices[0] && !meta.at("install").contains(options[0][choices[0]])) return false;
        if (choices[1]) {
            const auto value = meta.value("standalone", nlohmann::json());
            if (choices[1] == 3 ? !value.is_null() : !value.is_boolean() || value.get<bool>() != (choices[1] == 1)) return false;
        }
        if (choices[2] && meta.value("official_source", false) != (choices[2] == 1)) return false;
        if (choices[3] && !meta.value("latest", false)) return false;
        if (choices[4] && meta.value("source", "local") != options[4][choices[4]]) return false;
        if (choices[5] && meta.value("category", "runtime") != options[5][choices[5]]) return false;
        return true;
    }
};
}
