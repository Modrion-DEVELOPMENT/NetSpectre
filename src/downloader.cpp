// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and ADDITIONAL_TERMS.md.
#include "downloader.h"
#include "download_service.h"
#include <chrono>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <regex>
#include <set>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <utility>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace netspro {
    namespace {
        struct Program {
            std::string name, description, systems, version;
            nlohmann::json package;

            Program(std::string n, std::string d, std::string s, std::string v, nlohmann::json p = nullptr)
                : name(std::move(n)), description(std::move(d)), systems(std::move(s)), version(std::move(v)),
                  package(std::move(p)) {
            }
        };

        std::future<std::vector<Program> > onlineModels;

        std::vector<Program> cachedOllamaModels() {
            std::vector<Program> models;
            std::ifstream input("catalog/ollama-cache.json");
            if (!input) return models;
            try {
                nlohmann::json document;
                input >> document;
                for (const auto &value: document.at("models")) {
                    const auto name = value.get<std::string>();
                    if (!name.empty() && name.find_first_not_of(
                            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") == std::string::npos)
                        models.push_back({name, "Ollama model from offline cache", "Ollama", "latest"});
                }
            } catch (const std::exception &error) {
                std::cerr << "Ollama cache error: " << error.what() << '\n';
                models.clear();
            }
            return models;
        }

        void saveOllamaCache(const std::vector<Program> &models) {
            nlohmann::json document;
            document["models"] = nlohmann::json::array();
            for (const auto &model: models) document["models"].push_back(model.name);
#ifdef _WIN32
            const std::string temporary = "catalog/ollama-cache.json.tmp." + std::to_string(_getpid());
#else
            const std::string temporary = "catalog/ollama-cache.json.tmp." + std::to_string(getpid());
#endif
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) {
                std::cerr << "Cannot write Ollama cache.\n";
                return;
            }
            output << document.dump(2) << '\n';
            output.close();
            if (!output) {
                std::cerr << "Cannot save Ollama cache.\n";
                std::remove(temporary.c_str());
                return;
            }
#ifdef _WIN32
            const bool replaced = MoveFileExA(temporary.c_str(), "catalog/ollama-cache.json",
                                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
            const bool replaced = std::rename(temporary.c_str(), "catalog/ollama-cache.json") == 0;
#endif
            if (!replaced) {
                std::cerr << "Cannot save Ollama cache.\n";
                std::remove(temporary.c_str());
            }
        }

        std::string catalogStatus;
        nlohmann::json packageDocument;

        std::vector<Program> fetchOllamaModels() {
            std::vector<Program> result;
            std::set<std::string> seen;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
            for (int page = 1; page <= 100; ++page) {
                const auto html = downloads::fetch("https://ollama.com/search?page=" + std::to_string(page));
                for (const auto &name: downloads::parseLinks(html))
                    if (seen.insert(name).second)
                        result.push_back({
                            name, "Ollama model; open to load available tags and quantizations.", "Ollama", "select tag"
                        });
                if (html.find("hx-get=\"/search?page=" + std::to_string(page + 1) + "\"") == std::string::npos) break;
                if (page == 100 || std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("Ollama catalog refresh exceeded its limit; keeping the previous cache.");
            }
            if (result.empty()) throw
                    std::runtime_error("Ollama returned no models; its page layout may have changed.");
            return result;
        }

        std::vector<Program> &catalog() {
            static std::vector<Program> combined;
            static bool loaded = false;
            if (!loaded) {
                loaded = true;
                try {
                    std::ifstream input("catalog/third_party.json");
                    if (!input) throw std::runtime_error(
                        "Cannot open catalog/third_party.json. Start in the project directory.");
                    input >> packageDocument;
                    if (packageDocument.at("schema_version") != 1) throw std::runtime_error(
                        "Unsupported catalog schema.");
                    std::set<std::string> ids;
                    for (const auto &package: packageDocument.at("packages")) {
                        if (!ids.insert(package.at("id").get<std::string>()).second) throw std::runtime_error(
                            "Duplicate package ID.");
                        std::string systems;
                        for (const auto &item: package.at("install").items())
                            systems += (systems.empty() ? "" : ", ") + item.key();
                        combined.push_back({
                            package.at("name"), package.at("description"), systems, package.at("version"), package
                        });
                    }
                    for (const auto &model: packageDocument.value("ollama_models", nlohmann::json::array()))
                        combined.push_back({
                            model.at("id"), "Ollama model; open to load available tags and quantizations.", "Ollama",
                            "select tag"
                        });
                } catch (const std::exception &error) { catalogStatus = error.what(); }
                for (const auto &model: cachedOllamaModels()) {
                    if (std::none_of(combined.begin(), combined.end(), [&](const Program &p) {
                        return p.systems == "Ollama" && p.name == model.name;
                    }))
                        combined.push_back(model);
                }
            }
            return combined;
        }

        bool updateOnlineCatalog() {
            if (!onlineModels.valid() || onlineModels.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return false;
            try {
                const auto models = onlineModels.get();
                saveOllamaCache(models);
                for (const auto &model: models) {
                    auto &entries = catalog();
                    if (std::none_of(entries.begin(), entries.end(), [&](const Program &p) {
                        return p.systems == "Ollama" && p.name == model.name;
                    }))
                        entries.push_back(model);
                }
                catalogStatus = "Live Ollama catalog loaded. Select a model to browse its tags.";
            } catch (const std::exception &error) {
                catalogStatus = std::string(error.what()) + " Offline catalog retained.";
            }
            return true;
        }

        std::string lower(std::string value) {
            for (char &c: value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        }

        enum class Key {
            None, Character, Up, Down, Left, Right, Enter, Escape, Backspace,
            PageUp, PageDown, Mouse, Quit
        };

        struct Event {
            Key key = Key::None;
            char ch = 0;
            int x = 0, y = 0;
            bool click = false;
            int wheel = 0;
        };

        // Own the terminal mode and restore it even when rendering or saving fails.
        class Terminal {
#ifdef _WIN32
            HANDLE input_ = GetStdHandle(STD_INPUT_HANDLE), output_ = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD inputMode_ = 0, outputMode_ = 0;
#else
            termios original_{};
            int pendingByte_ = -1;
#endif

        public:
            Terminal() {
#ifdef _WIN32
                if (!GetConsoleMode(input_, &inputMode_) || !GetConsoleMode(output_, &outputMode_))
                    throw std::runtime_error("The downloader requires an interactive terminal.");
                if (!SetConsoleMode(output_, outputMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                    throw std::runtime_error("This console does not support ANSI output.");
                if (!SetConsoleMode(input_, ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT)) {
                    SetConsoleMode(output_, outputMode_);
                    throw std::runtime_error("Cannot enable console input.");
                }
#else
                if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &original_) != 0)
                    throw std::runtime_error("The downloader requires an interactive terminal.");
                termios raw = original_;
                raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
                raw.c_iflag &= ~(IXON | ICRNL);
                raw.c_cc[VMIN] = 0;
                raw.c_cc[VTIME] = 0;
                if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
                    throw std::runtime_error("Cannot enable terminal input.");
#endif
                std::cout << "\033[?1049h\033[?25l";
#ifndef _WIN32
                std::cout << "\033[?1003h\033[?1006h";
#endif
                std::cout.flush();
            }

            ~Terminal() {
                std::cout << "\033[?1003l\033[?1006l\033[0m\033[?25h\033[?1049l" << std::flush;
#ifdef _WIN32
                SetConsoleMode(input_, inputMode_);
                SetConsoleMode(output_, outputMode_);
#else
                tcsetattr(STDIN_FILENO, TCSANOW, &original_);
#endif
            }

            Terminal(const Terminal &) = delete;

            Terminal &operator=(const Terminal &) = delete;

            std::pair<int, int> size() const {
#ifdef _WIN32
                CONSOLE_SCREEN_BUFFER_INFO info{};
                if (GetConsoleScreenBufferInfo(output_, &info))
                    return {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
#else
                winsize size{};
                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col && size.ws_row)
                    return {size.ws_col, size.ws_row};
#endif
                return {80, 24};
            }

            Event readEvent() {
                Event event;
#ifdef _WIN32
                if (WaitForSingleObject(input_, 100) != WAIT_OBJECT_0) return event;
                INPUT_RECORD record{}; DWORD count = 0;
                if (!ReadConsoleInput(input_, &record, 1, &count)) {
                    event.key = Key::Quit;
                    return event;
                }
                if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                    const auto &key = record.Event.KeyEvent;
                    switch (key.wVirtualKeyCode) {
                        case VK_UP: event.key = Key::Up;
                            break;
                        case VK_DOWN: event.key = Key::Down;
                            break;
                        case VK_LEFT: event.key = Key::Left;
                            break;
                        case VK_RIGHT: event.key = Key::Right;
                            break;
                        case VK_PRIOR: event.key = Key::PageUp;
                            break;
                        case VK_NEXT: event.key = Key::PageDown;
                            break;
                        case VK_RETURN: event.key = Key::Enter;
                            break;
                        case VK_ESCAPE: event.key = Key::Escape;
                            break;
                        case VK_BACK: event.key = Key::Backspace;
                            break;
                        default: event.key = Key::Character;
                            event.ch = key.uChar.AsciiChar;
                    }
                    if (event.ch == 3) event.key = Key::Quit;
                } else if (record.EventType == MOUSE_EVENT) {
                    const auto &mouse = record.Event.MouseEvent;
                    CONSOLE_SCREEN_BUFFER_INFO info{};
                    GetConsoleScreenBufferInfo(output_, &info);
                    event.key = Key::Mouse;
                    event.x = mouse.dwMousePosition.X - info.srWindow.Left + 1;
                    event.y = mouse.dwMousePosition.Y - info.srWindow.Top + 1;
                    event.click = mouse.dwEventFlags == 0 && (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED);
                    if (mouse.dwEventFlags & MOUSE_WHEELED)
                        event.wheel = static_cast<SHORT>(HIWORD(mouse.dwButtonState)) > 0 ? -1 : 1;
                }
#else
                auto readByte = [this](int timeout) -> int {
                    if (pendingByte_ >= 0) {
                        const int value = pendingByte_;
                        pendingByte_ = -1;
                        return value;
                    }
                    pollfd descriptor{STDIN_FILENO, POLLIN, 0};
                    const int result = poll(&descriptor, 1, timeout);
                    if (result < 0) return errno == EINTR ? -1 : -2;
                    if (!result) return -1;
                    unsigned char c;
                    if (::read(STDIN_FILENO, &c, 1) == 1) return c;
                    return -2;
                };
                const int first = readByte(100);
                if (first == -2 || first == 3 || first == 4) {
                    event.key = Key::Quit;
                    return event;
                }
                if (first < 0) return event;
                if (first == 13 || first == 10) {
                    event.key = Key::Enter;
                    return event;
                }
                if (first == 127 || first == 8) {
                    event.key = Key::Backspace;
                    return event;
                }
                if (first != 27) {
                    event.key = Key::Character;
                    event.ch = static_cast<char>(first);
                    return event;
                }
                const int next = readByte(40);
                if (next != '[' && next != 'O') {
                    if (next >= 0) pendingByte_ = next;
                    event.key = Key::Escape;
                    return event;
                }
                std::string sequence;
                for (int i = 0; i < 48; ++i) {
                    const int c = readByte(40);
                    if (c < 0) break;
                    sequence += static_cast<char>(c);
                    if (c >= '@' && c <= '~') break;
                }
                if (sequence == "A") event.key = Key::Up;
                else if (sequence == "B") event.key = Key::Down;
                else if (sequence == "C") event.key = Key::Right;
                else if (sequence == "D") event.key = Key::Left;
                else if (sequence == "5~") event.key = Key::PageUp;
                else if (sequence == "6~") event.key = Key::PageDown;
                else if (!sequence.empty() && sequence[0] == '<') {
                    int button = 0;
                    char end = 0;
                    if (std::sscanf(sequence.c_str(), "<%d;%d;%d%c", &button, &event.x, &event.y, &end) == 4) {
                        event.key = Key::Mouse;
                        event.click = end == 'M' && button == 0;
                        if (button & 64) event.wheel = (button & 1) ? 1 : -1;
                    }
                }
#endif
                return event;
            }
        };

        class CatalogView {
            const std::string main_, accent_, input_;
            std::vector<std::size_t> matches_;
            std::ostringstream frame_;
            std::string lastFrame_;
            std::string query_, savedQuery_, status_;
            std::size_t selected_ = 0;
            bool searching_ = false, details_ = false;
            int requested_ = -1;
            int width_ = 80, height_ = 24, pageSize_ = 8, hoverX_ = 0, hoverY_ = 0;
            static constexpr int firstRow_ = 7;

            void filter() {
                matches_.clear();
                const auto needle = lower(query_);
                for (std::size_t i = 0; i < catalog().size(); ++i) {
                    const auto &p = catalog()[i];
                    if (lower(p.name + " " + p.description + " " + p.systems + " " + p.version).find(needle) !=
                        std::string::npos)
                        matches_.push_back(i);
                }
                selected_ = 0;
            }

            void line(int row, const std::string &text, const std::string &style = "") {
                const auto length = static_cast<std::size_t>(std::max(0, width_ - 3));
                std::string clipped = text;
                if (clipped.size() > length) clipped = clipped.substr(0, length > 3 ? length - 3 : 0) + "...";
                if (style == "\033[7m") clipped.resize(length, ' ');
                frame_ << "\033[" << row << ";2H\033[0m" << main_ << style << clipped << "\033[0m" << main_;
            }

            void button(int row, int column, const std::string &text) {
                const bool hovered = hoverY_ == row && hoverX_ >= column && hoverX_ < column + static_cast<int>(text.
                                         size());
                frame_ << "\033[" << row << ';' << column << "H\033[0m" << accent_
                        << (hovered ? "\033[7m" : "") << text << "\033[0m" << main_;
            }

            void present() {
                const std::string frame = frame_.str();
                if (frame != lastFrame_) {
                    std::cout << frame << std::flush;
                    lastFrame_ = frame;
                }
            }

            void move(int delta) {
                if (matches_.empty()) return;
                const int next = static_cast<int>(selected_) + delta;
                selected_ = static_cast<std::size_t>(
                    std::max(0, std::min(next, static_cast<int>(matches_.size() - 1))));
                status_.clear();
            }

            void download() {
                if (!matches_.empty()) requested_ = static_cast<int>(matches_.at(selected_));
            }

        public:
            CatalogView(std::string mainColor, std::string accentColor, std::string inputColor)
                : main_(std::move(mainColor)), accent_(std::move(accentColor)), input_(std::move(inputColor)) {
                filter();
            }

            bool hasRequest() const { return requested_ >= 0; }

            void refresh() {
                const auto previous = matches_.empty() ? catalog().size() : matches_[selected_];
                filter();
                const auto found = std::find(matches_.begin(), matches_.end(), previous);
                if (found != matches_.end()) selected_ = static_cast<std::size_t>(found - matches_.begin());
                lastFrame_.clear();
            }

            void performRequest() {
                const auto p = catalog().at(static_cast<std::size_t>(requested_));
                requested_ = -1;
                try {
                    if (p.systems == "Ollama" && p.name.find(':') == std::string::npos) {
                        std::cout << "Loading tags for " << p.name << "..." << std::endl;
                        auto tags = downloads::variants(p.name, status_);
                        if (tags.empty()) {
                            for (const auto &model: packageDocument.value("ollama_models", nlohmann::json::array()))
                                if (model.at("id") == p.name)
                                    for (const auto &tag: model.at("variants")) tags.push_back(tag.at("tag"));
                            if (!tags.empty()) status_ += " Using bundled fallback tags.";
                        }
                        if (tags.empty()) throw std::runtime_error(status_ + " No cached tags available.");
                        auto &entries = catalog();
                        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Program &entry) {
                            return entry.systems == "Ollama" && entry.name.compare(0, p.name.size() + 1, p.name + ":")
                                   == 0;
                        }), entries.end());
                        for (const auto &tag: tags)
                            entries.push_back({
                                tag, "Ollama model variant; exact tag includes size/quantization when provided.",
                                "Ollama", tag.substr(tag.find(':') + 1)
                            });
                        query_ = p.name + ":";
                        details_ = false;
                        filter();
                    } else {
                        status_ = p.systems == "Ollama"
                                      ? downloads::pull(p.name, packageDocument)
                                      : downloads::install(p.package, packageDocument);
                        std::cout << status_ << "\nPress Enter to return to the catalog." << std::flush;
                        std::string ignored;
                        std::getline(std::cin, ignored);
                    }
                } catch (const std::exception &error) {
                    status_ = "Error: " + std::string(error.what());
                    std::cout << status_ << "\nPress Enter to return to the catalog." << std::flush;
                    std::string ignored;
                    std::getline(std::cin, ignored);
                }
                lastFrame_.clear();
            }

            void render(int width, int height) {
                frame_.str("");
                frame_.clear();
                width_ = width;
                height_ = height;
                pageSize_ = std::max(1, (height_ - 12) / 3);
                frame_ << "\033[0m\033[2J\033[H" << main_;
                if (width_ < 64 || height_ < 18) {
                    line(1, "Downloader: resize terminal to at least 64 x 18.", accent_);
                    line(3, "Press Q or Escape to return.");
                    present();
                    return;
                }
                line(1, "MODRION NETSPECTRE / DOWNLOAD CATALOG", accent_);
                button(2, 2, "[ Search / ]");
                if (!details_) {
                    button(2, 17, "[ Previous < ]");
                    button(2, 34, "[ Next > ]");
                }
                button(2, 47, "[ Close Q ]");
                line(3, "PACKAGES / OLLAMA - real installs; commands require confirmation.");
                line(4, (searching_ ? "Search> " : "Search: ") + query_ + (searching_ ? "_" : ""),
                     searching_ ? input_ : "");
                if (searching_) line(5, "Type to filter | Backspace to edit | Enter to accept | Escape to cancel");
                else line(5, "Arrows: select/page | Enter: details | /: search | Mouse: hover/click/scroll");
                if (details_ && !matches_.empty()) {
                    const auto &p = catalog()[matches_[selected_]];
                    line(7, p.name, accent_);
                    line(9, p.description);
                    line(11, "Supported OS: " + p.systems);
                    line(12, "Version: " + p.version);
                    button(14, 2, p.systems == "Ollama" && p.name.find(':') == std::string::npos
                                      ? "[ Browse tags: Enter ]"
                                      : "[ Install: Enter ]");
                    button(14, 34, "[ Back: B / Escape ]");
                    if (!p.package.is_null())
                        line(15, "Requirements: " + p.package.value("requirements", nlohmann::json::array()).dump());
                } else {
                    const std::size_t start = (selected_ / pageSize_) * pageSize_;
                    for (std::size_t i = start; i < matches_.size() && i < start + pageSize_; ++i) {
                        const auto &p = catalog()[matches_[i]];
                        const int row = firstRow_ + static_cast<int>(i - start) * 3;
                        const auto style = i == selected_ ? "\033[7m" : "";
                        line(row, (i == selected_ ? "> " : "  ") + p.name + " | v" + p.version, style);
                        line(row + 1, "  " + p.description, style);
                        line(row + 2, "  OS: " + p.systems, style);
                    }
                    if (matches_.empty()) line(7, "No matches. Press / to change your search.", accent_);
                }
                const std::size_t pages = std::max<std::size_t>(1, (matches_.size() + pageSize_ - 1) / pageSize_);
                line(height_ - 3, "Page " + std::to_string(selected_ / pageSize_ + 1) + " / " + std::to_string(pages) +
                                  " | " + std::to_string(matches_.size()) + " matches" + (details_ ? " | Details" : ""),
                     accent_);
                line(height_ - 1, status_.empty() ? catalogStatus : status_);
                present();
            }

            bool handle(const Event &event) {
                if (event.key == Key::Mouse) {
                    hoverX_ = event.x;
                    hoverY_ = event.y;
                } else if (event.key != Key::None) {
                    hoverX_ = 0;
                    hoverY_ = 0;
                }
                if (event.key == Key::Quit) return false;
                if (searching_) {
                    if (event.key == Key::Escape) {
                        query_ = savedQuery_;
                        searching_ = false;
                        filter();
                    } else if (event.key == Key::Enter) searching_ = false;
                    else if (event.key == Key::Backspace && !query_.empty()) {
                        query_.pop_back();
                        filter();
                    } else if (event.key == Key::Character && event.ch >= 32 && event.ch <= 126 && query_.size() < 80) {
                        query_ += event.ch;
                        filter();
                    }
                    return true;
                }
                if (event.key == Key::Escape) {
                    if (details_) details_ = false;
                    else return false;
                }
                if (event.key == Key::Character) {
                    if (event.ch == 'q' || event.ch == 'Q') return false;
                    if (event.ch == '/') {
                        searching_ = true;
                        savedQuery_ = query_;
                        details_ = false;
                    }
                    if (event.ch == 'b' || event.ch == 'B') details_ = false;
                }
                if (width_ < 64 || height_ < 18) return true;
                if (event.key == Key::Enter && !matches_.empty()) {
                    if (details_) download();
                    else {
                        details_ = true;
                        status_.clear();
                    }
                }
                if (!details_) {
                    if (event.key == Key::Up) move(-1);
                    if (event.key == Key::Down) move(1);
                    if (event.key == Key::Left || event.key == Key::PageUp) move(-pageSize_);
                    if (event.key == Key::Right || event.key == Key::PageDown) move(pageSize_);
                }
                if (event.key == Key::Mouse) {
                    if (event.click && event.y == 2) {
                        if (event.x >= 2 && event.x <= 13) {
                            searching_ = true;
                            savedQuery_ = query_;
                            details_ = false;
                        } else if (event.x >= 17 && event.x <= 30 && !details_) move(-pageSize_);
                        else if (event.x >= 34 && event.x <= 43 && !details_) move(pageSize_);
                        else if (event.x >= 47 && event.x <= 57) return false;
                    } else if (details_) {
                        if (event.click && event.y == 14 && event.x >= 2 && event.x <= 30) download();
                        else if (event.click && event.y == 14 && event.x >= 34 && event.x <= 53) details_ = false;
                    } else if (event.wheel) move(event.wheel);
                    else if (event.x >= 2 && event.x < width_ && event.y >= firstRow_ && event.y < firstRow_ + pageSize_
                             * 3) {
                        const std::size_t index = (selected_ / pageSize_) * pageSize_ + (event.y - firstRow_) / 3;
                        if (index < matches_.size()) {
                            selected_ = index;
                            if (event.click) {
                                details_ = true;
                                status_.clear();
                            }
                        }
                    }
                }
                return true;
            }
        };
    }

    void startCatalogRefresh() {
        if (!onlineModels.valid()) onlineModels = std::async(std::launch::async, fetchOllamaModels);
    }

    void openDownloader(const std::string &mainColor, const std::string &accentColor,
                        const std::string &inputColor) {
        CatalogView view(mainColor, accentColor, inputColor);
        bool running = true;
        while (running) {
            {
                Terminal terminal;
                auto dimensions = terminal.size();
                view.render(dimensions.first, dimensions.second);
                while (running && !view.hasRequest()) {
                    const Event event = terminal.readEvent();
                    running = view.handle(event);
                    const auto nextSize = terminal.size();
                    const bool updated = updateOnlineCatalog();
                    if (updated) view.refresh();
                    if (event.key != Key::None || nextSize != dimensions || updated) {
                        dimensions = nextSize;
                        view.render(dimensions.first, dimensions.second);
                    }
                }
            }
            // Installation tools need normal terminal input for progress and password prompts.
            if (running && view.hasRequest()) view.performRequest();
        }
    }
}
