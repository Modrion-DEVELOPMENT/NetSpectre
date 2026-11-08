// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and ADDITIONAL_TERMS.md.
#pragma once
#include <string>

namespace netspro {
    void startCatalogRefresh();

    void openDownloader(const std::string &mainColor, const std::string &accentColor,
                        const std::string &inputColor);
}
