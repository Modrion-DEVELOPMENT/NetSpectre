// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace netspro {
    namespace downloads {
        using Command = std::vector<std::string>;

        std::string platform();

        int run(const Command &command, std::string *output = nullptr);

        std::string fetch(const std::string &url);

        std::vector<std::string> parseLinks(const std::string &html, const std::string &model = "");

        std::vector<std::string> variants(const std::string &model, std::string &status);

        std::string install(const nlohmann::json &package, const nlohmann::json &document);

        std::string pull(const std::string &tag, const nlohmann::json &document);
    }
}
