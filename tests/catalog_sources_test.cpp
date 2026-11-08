// SPDX-License-Identifier: AGPL-3.0-only
#include "catalog_sources.h"
#include "catalog_filters.h"
#include <iostream>
#include <stdexcept>
using nlohmann::json;
void require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
int main() {
    try {
        json ranking={{"start_date","2026-10-01"},{"end_date","2026-10-30"},{"items",json::array()}};
        json formulae=json::array();
        ranking["items"].push_back({{"formula","--malicious"},{"count","999"}});
        ranking["items"].push_back({{"formula","third-party/tap/tool"},{"count","888"}});
        for(int i=0;i<201;++i) {
            std::string name="package"+std::to_string(i);
            ranking["items"].push_back({{"formula",name},{"count","100"}});
            formulae.push_back({{"name",name},{"desc","Test\033[31m description"},{"versions",{{"stable","1.2.3"}}},
                {"disabled",i==0},{"bottle",{{"stable",{{"files",{{"arm64_sequoia",json::object()},{"x86_64_linux",json::object()}}}}}}}});
        }
        auto data=netspro::downloads::buildHomebrewCatalog(ranking,formulae);
        require(data["packages"].size()==200,"must return 200 eligible packages");
        auto first=data["packages"][0];
        require(first["id"]=="brew:package1","skip disabled packages and reject arbitrary taps/options");
        require(first["install"]["linux"]==json({"brew","install","--formula","package1"}),"fixed installation recipe");
        require(first["description"].get<std::string>().find('\033')==std::string::npos,"terminal escape sanitization");
        netspro::CatalogFilters filters;
        require(filters.accepts(first),"default filters");
        filters.choices[0]=1; require(!filters.accepts(first),"exclude native Windows");
        filters.choices[0]=3; require(filters.accepts(first),"Linux bottle");
        filters.choices[1]=1; require(!filters.accepts(first),"unknown is not standalone");
        filters.choices[1]=3; require(filters.accepts(first),"unknown filter");
        filters.choices[2]=2; require(!filters.accepts(first),"official source filter");
        filters.choices[2]=1;filters.choices[3]=1; require(filters.accepts(first),"latest official");
        first["latest"]=false;require(!filters.accepts(first),"older version excluded");
        filters.cycle(0,-1);require(filters.choices[0]==2,"reverse cycling");
        bool failed=false;ranking["items"]=json::array();
        try { netspro::downloads::buildHomebrewCatalog(ranking,formulae); } catch(const std::exception&) { failed=true; }
        require(failed,"empty upstream response must not replace cache");
        std::cout << "Catalog source and filter tests passed.\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
