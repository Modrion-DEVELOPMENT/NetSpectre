// SPDX-License-Identifier: AGPL-3.0-only
#include "download_service.h"
#include <cerrno>
#include <cctype>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#else
#include <spawn.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace netspro {
    namespace downloads {
        std::string platform() {
#ifdef _WIN32
            return "windows";
#elif defined(__APPLE__)
            return "macos";
#else
            return "linux";
#endif
        }

        namespace {
            bool validName(const std::string &name, bool tag = false) {
                return !name.empty() && name.size() < 200 && std::isalnum(static_cast<unsigned char>(name[0])) &&
                       name.find_first_not_of(tag
                                                  ? "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-:"
                                                  : "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")
                       == std::string::npos;
            }

            std::string display(const Command &command) {
                std::string result;
                for (const auto &arg: command) result += (result.empty() ? "" : " ") + nlohmann::json(arg).dump();
                return result;
            }
#ifdef _WIN32
BOOL WINAPI ignoreInterrupt(DWORD event) { return event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT; }
            std::string quote(const std::string &arg) {
                std::string result = "\"";
                unsigned slashes = 0;
                for (char c: arg) {
                    if (c == '\\') {
                        ++slashes;
                        continue;
                    }
                    result.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
                    slashes = 0;
                    result += c;
                }
                result.append(slashes * 2, '\\');
                return result + '"';
            }
#endif
        }

        // Argument vectors are passed directly to the OS, never interpreted by a shell.
        int run(const Command &command, std::string *output) {
            if (command.empty() || command[0].empty()) throw std::runtime_error("Empty install command.");
            for (const auto &arg: command)
                if (arg.find('\0') != std::string::npos) throw std::runtime_error("Invalid command argument.");
            FILE *capture = output ? std::tmpfile() : nullptr;
            if (output && !capture) throw std::runtime_error("Cannot create process output file.");
            int result = 127;
#ifdef _WIN32
            std::string line;
            for (const auto &arg: command) line += (line.empty() ? "" : " ") + quote(arg);
            STARTUPINFOA info{}; info.cb = sizeof(info);
            PROCESS_INFORMATION child{};
            HANDLE inherited = nullptr;
            if (capture) {
                if (!DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(capture))),
                                     GetCurrentProcess(), &inherited, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                    std::fclose(capture);
                    throw std::runtime_error("Cannot capture process output.");
                }
                info.dwFlags = STARTF_USESTDHANDLES;
                info.hStdOutput = info.hStdError = inherited;
                info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            }
            std::vector<char> buffer(line.begin(), line.end()); buffer.push_back(0);
            if (!capture) SetConsoleCtrlHandler(ignoreInterrupt, TRUE);
            if (CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, capture != nullptr, 0, nullptr, nullptr, &info,
                               &child)) {
                WaitForSingleObject(child.hProcess, INFINITE);
                DWORD code = 1;
                GetExitCodeProcess(child.hProcess, &code);
                result = static_cast<int>(code);
                CloseHandle(child.hProcess);
                CloseHandle(child.hThread);
            }
            if (!capture) SetConsoleCtrlHandler(ignoreInterrupt, FALSE);
            if (inherited) CloseHandle(inherited);
#else
            std::vector<char *> args;
            for (const auto &arg: command) args.push_back(const_cast<char *>(arg.c_str()));
            args.push_back(nullptr);
            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);
            if (capture) {
                posix_spawn_file_actions_adddup2(&actions, fileno(capture), STDOUT_FILENO);
                posix_spawn_file_actions_adddup2(&actions, fileno(capture), STDERR_FILENO);
            }
            posix_spawnattr_t attributes;
            posix_spawnattr_init(&attributes);
            sigset_t defaults;
            sigemptyset(&defaults);
            sigaddset(&defaults, SIGINT);
            posix_spawnattr_setsigdefault(&attributes, &defaults);
            posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF);
            struct sigaction previous{}, ignored{};
            ignored.sa_handler = SIG_IGN;
            sigemptyset(&ignored.sa_mask);
            if (!capture) sigaction(SIGINT, &ignored, &previous);
            pid_t child;
            const int spawned = posix_spawnp(&child, args[0], &actions, &attributes, args.data(), environ);
            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attributes);
            if (!spawned) {
                int status = 0;
                pid_t waited;
                do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
                if (waited >= 0) result = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            }
            if (!capture) sigaction(SIGINT, &previous, nullptr);
#endif
            if (capture) {
                output->clear();
                std::rewind(capture);
                char data[4096];
                std::size_t count;
                while ((count = std::fread(data, 1, sizeof(data), capture)) != 0) {
                    if (output->size() + count > 8 * 1024 * 1024) {
                        result = 1;
                        break;
                    }
                    output->append(data, count);
                }
                std::fclose(capture);
            }
            return result;
        }

        std::string fetch(const std::string &url) {
            if (url.compare(0, 19, "https://ollama.com/") != 0)
                throw std::runtime_error("Only official Ollama catalog URLs are supported.");
            std::string output;
            if (run({
                        "curl", "--fail", "--silent", "--show-error", "--location", "--proto", "=https",
                        "--proto-redir", "=https", "--connect-timeout", "4", "--max-time", "15",
                        "--max-filesize", "8000000", "--header", "HX-Request: true", url
                    }, &output) != 0)
                throw std::runtime_error("Ollama request failed. Check your connection and curl installation.");
            return output;
        }

        std::vector<std::string> parseLinks(const std::string &html, const std::string &model) {
            if (!model.empty() && !validName(model)) throw std::runtime_error("Invalid model name.");
            const std::regex pattern("href=[\"']/library/([A-Za-z0-9._:-]+)[\"']");
            std::set<std::string> seen;
            std::vector<std::string> result;
            for (std::sregex_iterator it(html.begin(), html.end(), pattern), end; it != end; ++it) {
                const std::string name = (*it)[1];
                const bool matches = model.empty()
                                         ? name.find(':') == std::string::npos
                                         : name.compare(0, model.size() + 1, model + ":") == 0;
                if (matches && validName(name, !model.empty()) && seen.insert(name).second) result.push_back(name);
            }
            return result;
        }

        std::vector<std::string> variants(const std::string &model, std::string &status) {
            if (!validName(model)) throw std::runtime_error("Invalid model name.");
            const std::string path = "catalog/ollama-tags-" + model + ".json";
            std::vector<std::string> tags;
            try {
                tags = parseLinks(fetch("https://ollama.com/library/" + model + "/tags"), model);
                if (tags.empty()) throw std::runtime_error("No model tags found; Ollama's page may have changed.");
                status = "Live tags from ollama.com";
                const std::string temporary = path + ".tmp." + std::to_string(
#ifdef _WIN32
                                                  _getpid()
#else
                                                  getpid()
#endif
                                              );
                std::ofstream file(temporary);
                file << nlohmann::json(tags).dump(2);
                file.close();
#ifdef _WIN32
                const bool saved = file && MoveFileExA(temporary.c_str(), path.c_str(),
                                                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
                const bool saved = file && std::rename(temporary.c_str(), path.c_str()) == 0;
#endif
                if (!saved) {
                    std::remove(temporary.c_str());
                    status += "; cache could not be saved";
                }
            } catch (const std::exception &error) {
                status = std::string(error.what()) + " Using offline tags.";
                std::ifstream file(path);
                if (file) {
                    nlohmann::json data;
                    file >> data;
                    for (const auto &item: data) {
                        const auto tag = item.get<std::string>();
                        if (validName(tag, true) && tag.compare(0, model.size() + 1, model + ":") == 0) tags.
                                push_back(tag);
                    }
                }
            }
            return tags;
        }

        namespace {
            struct Step {
                std::string name;
                std::vector<Command> commands;
                Command verify;
            };

            using Plan = std::vector<Step>;

            Command commandFor(const nlohmann::json &entry, const std::string &field) {
                if (!entry.contains(field) || !entry.at(field).contains(platform())) return {};
                return entry.at(field).at(platform()).get<Command>();
            }

            std::vector<Command> commandsFor(const nlohmann::json &entry) {
                if (!entry.contains("install") || !entry.at("install").contains(platform()))
                    throw std::runtime_error(
                        "No installation command for " + platform() + ": " + entry.at("name").get<std::string>());
                const auto &value = entry.at("install").at(platform());
                if (!value.is_array() || value.empty()) throw std::runtime_error("Invalid install command.");
                if (value[0].is_string()) return {value.get<Command>()};
                return value.get<std::vector<Command> >();
            }

            void resolve(const nlohmann::json &entry, const nlohmann::json &document, Plan &plan,
                         std::set<std::string> &active, std::set<std::string> &done, bool dependency) {
                const auto id = entry.at("id").get<std::string>();
                if (done.count(id)) return;
                if (!active.insert(id).second) throw std::runtime_error("Dependency cycle: " + id);
                const Command check = commandFor(entry, "check");
                std::string output;
                if (dependency && !check.empty() && run(check, &output) == 0) {
                    active.erase(id);
                    done.insert(id);
                    return;
                }
                for (const auto &requirement: entry.value("requirements", std::vector<std::string>{})) {
                    bool found = false;
                    for (const auto &candidate: document.at("packages"))
                        if (candidate.at("id") == requirement) {
                            resolve(candidate, document, plan, active, done, true);
                            found = true;
                            break;
                        }
                    if (!found) throw std::runtime_error("Unknown requirement: " + requirement);
                }
                plan.push_back({entry.at("name").get<std::string>(), commandsFor(entry), check});
                active.erase(id);
                done.insert(id);
            }

            class InstallerFile {
                std::string path_;

            public:
                InstallerFile() {
#ifdef _WIN32
                    char directory[MAX_PATH], filename[MAX_PATH];
                    if (!GetTempPathA(MAX_PATH, directory) || !GetTempFileNameA(directory, "nsp", 0, filename))
                        throw std::runtime_error("Cannot create installer temporary file.");
                    path_ = filename;
#else
                    const char *directory = std::getenv("TMPDIR");
                    std::string pattern = std::string(directory ? directory : "/tmp") + "/netspro-installer-XXXXXX";
                    std::vector<char> name(pattern.begin(), pattern.end());
                    name.push_back(0);
                    const int fd = mkstemp(name.data());
                    if (fd < 0) throw std::runtime_error("Cannot create installer temporary file.");
                    close(fd);
                    path_ = name.data();
#endif
                }

                ~InstallerFile() { if (!path_.empty()) std::remove(path_.c_str()); }
                const std::string &path() const { return path_; }
            };

            std::string execute(Plan plan) {
                std::cout << "\nInstallation plan (" << platform() << "):\n";
                for (const auto &step: plan) {
                    std::cout << "\n" << step.name << ":\n";
                    for (const auto &command: step.commands) std::cout << "  " << display(command) << '\n';
                }
                std::cout <<
                        "\nCommands come from catalog/third_party.json. System installs may request administrator rights.\n"
                        "Type INSTALL to proceed, or press Enter to cancel: " << std::flush;
                std::string answer;
                if (!std::getline(std::cin, answer) || answer != "INSTALL") return "Installation cancelled.";
                std::unique_ptr<InstallerFile> installerFile;
                for (auto &step: plan)
                    for (auto &command: step.commands)
                        for (auto &arg: command)
                            if (arg == "{ollama_installer}") {
                                if (!installerFile) installerFile.reset(new InstallerFile());
                                arg = installerFile->path();
                            }
                for (const auto &step: plan) {
                    std::cout << "\nInstalling " << step.name << "...\n";
                    for (const auto &command: step.commands) {
                        std::cout << "> " << display(command) << std::endl;
                        const int code = run(command);
                        if (code != 0)
                            throw std::runtime_error("Command failed (exit " + std::to_string(code) + "): " +
                                                     display(command) +
                                                     ". Previous steps may have completed; no rollback was attempted.");
                    }
                    if (!step.verify.empty()) {
                        std::string output;
                        if (run(step.verify, &output) != 0)
                            throw std::runtime_error("Installed " + step.name +
                                                     ", but verification failed. Check PATH and restart the terminal if needed.");
                    }
                }
                return "Installation completed successfully.";
            }
        }

        std::string install(const nlohmann::json &package, const nlohmann::json &document) {
            Plan plan;
            std::set<std::string> active, done;
            resolve(package, document, plan, active, done, false);
            return execute(std::move(plan));
        }

        std::string pull(const std::string &tag, const nlohmann::json &document) {
            if (!validName(tag, true)) throw std::runtime_error("Invalid Ollama tag.");
            auto entry = nlohmann::json{
                {"id", "model:" + tag}, {"name", tag}, {"requirements", {"ollama"}},
                {"install", {{platform(), {"ollama", "pull", tag}}}}
            };
            std::cout << "Ollama must be running (start the Ollama app or run ollama serve).\n"
                    "Models can require several GB of disk space. Ctrl+C cancels the running command.\n";
            return install(entry, document);
        }
    }
}
