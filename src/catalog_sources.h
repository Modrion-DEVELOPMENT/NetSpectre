// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nlohmann/json.hpp>
namespace netspro { namespace downloads {
nlohmann::json buildHomebrewCatalog(const nlohmann::json& ranking, const nlohmann::json& formulae);
nlohmann::json fetchHomebrewCatalog();
}}
