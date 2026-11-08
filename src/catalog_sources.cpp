// SPDX-License-Identifier: AGPL-3.0-only
#include "catalog_sources.h"
#include "download_service.h"
#include <map>
#include <regex>
#include <set>
#include <stdexcept>

namespace netspro { namespace downloads {
namespace {
std::string clean(std::string text) {
    for (char& c : text) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
    return text.substr(0, 500);
}
}
nlohmann::json buildHomebrewCatalog(const nlohmann::json& ranking, const nlohmann::json& formulae) {
    using nlohmann::json;
    std::map<std::string, const json*> metadata;
    for (const auto& formula : formulae) metadata.emplace(formula.at("name").get<std::string>(), &formula);
    json packages = json::array();
    std::set<std::string> seen;
    const std::regex safe("[a-z0-9][a-z0-9+_.@-]*");
    for (const auto& item : ranking.at("items")) {
        const auto name = item.at("formula").get<std::string>();
        // Only official core names, no taps, options or remotely supplied commands.
        if (!std::regex_match(name, safe) || !seen.insert(name).second || !metadata.count(name)) continue;
        const auto& formula = *metadata.at(name);
        if (formula.value("disabled", false)) continue;
        json install = json::object();
        const auto files = formula.value("bottle", json::object()).value("stable", json::object()).value("files", json::object());
        for (const auto& bottle : files.items()) {
            const std::string os = bottle.key().find("linux") != std::string::npos ? "linux" : "macos";
            install[os] = {"brew", "install", "--formula", name};
        }
        if (install.empty()) continue; // Do not claim OS support without bottle metadata.
        const auto stable = formula.at("versions").at("stable");
        if (!stable.is_string()) continue;
        packages.push_back({{"id", "brew:" + name}, {"name", "Homebrew: " + name},
            {"description", clean(formula.value("desc", std::string{}))},
            {"version", clean(stable.get<std::string>())}, {"source", "homebrew"},
            {"category", "formula"}, {"official_source", true}, {"standalone", nullptr},
            {"latest", !formula.value("deprecated", false) && name.find('@') == std::string::npos}, {"rank", packages.size()+1},
            {"install_count", item.at("count")}, {"requirements", {"homebrew"}}, {"install", install}});
        if (packages.size() == 200) break;
    }
    if (packages.size() != 200) throw std::runtime_error("Homebrew returned fewer than 200 supported formulae; keeping the previous catalog.");
    return {{"schema_version", 1}, {"source", "https://formulae.brew.sh/api/analytics/install-on-request/30d.json"},
        {"start_date", ranking.at("start_date")}, {"end_date", ranking.at("end_date")}, {"packages", packages}};
}
nlohmann::json fetchHomebrewCatalog() {
    const auto ranking = nlohmann::json::parse(fetch("https://formulae.brew.sh/api/analytics/install-on-request/30d.json"));
    const auto formulae = nlohmann::json::parse(fetch("https://formulae.brew.sh/api/formula.json"));
    return buildHomebrewCatalog(ranking, formulae);
}
}}
