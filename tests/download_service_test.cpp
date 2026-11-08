// SPDX-License-Identifier: AGPL-3.0-only
#include "download_service.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using nlohmann::json;
using namespace netspro::downloads;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }

int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "echo") {
            for (int i = 2; i < argc; ++i) std::cout << argv[i] << '\n';
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "fail") return 23;
        std::string output;
        require(run({argv[0], "echo", "with spaces", "$(touch SHOULD_NOT_EXIST);\"'"}, &output) == 0, "process launch");
        require(output == "with spaces\n$(touch SHOULD_NOT_EXIST);\"'\n", "literal argv preservation");
        require(run({argv[0], "fail"}, &output) == 23, "exit code propagation");
        require(run({"netspro-command-that-does-not-exist"}, &output) != 0, "missing executable");
        auto names = parseLinks(
            "<a href=\"/library/llama3.2\"><a href='/library/llama3.2'><a href='/library/x:latest'><a href='/library/../bad'>");
        require(names == std::vector<std::string>{"llama3.2"}, "model parsing/deduplication");
        auto tags = parseLinks(
            "<a href='/library/llama3.2:1b-q4_K_M'><a href='/library/other:latest'><a href='/library/llama3X2:bad'>",
            "llama3.2");
        require(tags == std::vector<std::string>{"llama3.2:1b-q4_K_M"}, "exact tag model matching");
        json dependency = {
            {"id", "dependency"}, {"name", "Dependency"},
            {"install", {{platform(), {argv[0], "echo", "dependency installed"}}}}
        };
        json package = {
            {"id", "package"}, {"name", "Package"}, {"requirements", {"dependency"}},
            {"install", {{platform(), {argv[0], "echo", "package installed"}}}}
        };
        json document = {{"packages", {dependency, package}}};
        std::istringstream confirm("INSTALL\n");
        auto *input = std::cin.rdbuf(confirm.rdbuf());
        require(install(package, document) == "Installation completed successfully.", "dependency installation");
        std::cin.rdbuf(input);
        std::istringstream cancel("\n");
        input = std::cin.rdbuf(cancel.rdbuf());
        require(install(package, document) == "Installation cancelled.", "cancel plan");
        std::cin.rdbuf(input);
        package["requirements"] = {"missing"};
        bool rejected = false;
        try { install(package, document); } catch (const std::exception &) { rejected = true; }
        require(rejected, "unknown dependency");
        dependency["requirements"] = {"package"};
        package["requirements"] = {"dependency"};
        document["packages"] = {dependency, package};
        rejected = false;
        try { install(package, document); } catch (const std::exception &) { rejected = true; }
        require(rejected, "dependency cycle");
        rejected = false;
        try { pull("../bad;command", document); } catch (const std::exception &) { rejected = true; }
        require(rejected, "unsafe model tag");
        package["requirements"] = json::array();
        package["install"][platform()] = {argv[0], "fail"};
        std::istringstream failure("INSTALL\n");
        input = std::cin.rdbuf(failure.rdbuf());
        rejected = false;
        try { install(package, document); } catch (const std::exception &e) {
            rejected = std::string(e.what()).find("exit 23") != std::string::npos;
        }
        std::cin.rdbuf(input);
        require(rejected, "installation failure propagation");
        std::cout << "Download service tests passed.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
