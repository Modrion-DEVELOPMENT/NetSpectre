//
//  main.cpp
//  Netspro
//
//  Created by Hendrik Hanking on 31.12.25.
//  COPYRIGHT Hendrik Vincent Hanking 2025
//  All rights reserved
//

#include <chrono>
#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <random>
#include <stdexcept>
#include <nlohmann/json.hpp>

// ===================== Declarations =====================
void clearTerminal();
void readUserInput();
void showMainScreen();
void processInput();
std::string getSerialNumber();
void login();
void initializeTokens();
void logo();
bool dev_mode = false;
bool developer = false;
bool pro_basic = false;
// AkteViewer
void fakeLoading(double zeit);
void openRecordInteractive();
void openRecordFile(const std::string& fileName);
std::string createNewRecord();
// Crypto / File Utils
std::string encryptText(const std::string& text);
std::string decryptText(const std::string& text);
std::string shiftLetters(const std::string& input_text, const std::vector<int>& shifts);
std::string unshiftLetters(const std::string& text, const std::vector<int>& shifts);
std::string trim(const std::string& s);
std::string to_lower(std::string s);
std::string to_upper(std::string s);
std::string remove_spaces(const std::string& s);
std::string read_file_utf8(const std::string& path);
void write_file_utf8(const std::string& path, const std::string& content);
nlohmann::json readJsonFile(const std::string& path);
void writeJsonFile(const std::string& path, const nlohmann::json& document);
void createJsonFile(const std::string& path, const nlohmann::json& initialDocument);
std::string buildInitials(const std::string& fullName);
std::string shuffle_string(std::string s);
long long random_int64(long long lo, long long hi);
std::mt19937& rng();
bool endsWith(const std::string& str, const std::string& suffix);

// ===================== Variables =====================
std::string inputLine;
std::string version = "DEV1.1.1.4.ax20262B";
std::string short_version = "DEV1.1.1.4";
std::string serialNumber;
std::string username;
std::string password;
std::string composedToken;
nlohmann::json appConfig;
std::vector<std::string> userTokens;
std::vector<std::string> proBasicTokens;
std::vector<std::string> devTokens;

static const std::vector<int> SHIFT = {9, 9, 4, 13, 2, 10, 3};


// ===================== Main =====================
int main() {
    try {
        createJsonFile("config.json", {
            {"version", 1},
            {"settings", nlohmann::json::object()}
        });
        appConfig = readJsonFile("config.json");
    } catch (const std::exception& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 1;
    }

    clearTerminal();
    initializeTokens();
    serialNumber = getSerialNumber();
    login();

    while (true) {
        readUserInput();
        if (trim(inputLine) == "exit") break;
        processInput();
    }

    return 0;
}

// ===================== System Infos =====================
std::string getSerialNumber() {
    std::string result;


#if defined(_WIN32) || defined(_WIN64)


    char buffer[256];

    // moderner Ersatz für WMIC
    const char* cmd =
    "powershell -NoProfile -Command \"(Get-CimInstance Win32_BIOS).SerialNumber\"";

    FILE* pipe = _popen(cmd, "r");
    if (!pipe) return "Unknown";

    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }

    _pclose(pipe);

#elif defined(__APPLE__)

    char buffer[256];
    FILE* pipe = popen(
        "system_profiler SPHardwareDataType | awk '/Serial/ {print $4}'", "r"
    );

    if (!pipe) return "Unknown";

    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }

    pclose(pipe);

#elif defined(__linux__)

    char buffer[256];
    FILE* pipe = popen("cat /sys/class/dmi/id/product_serial", "r");

    if (!pipe) return "Unknown";

    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }
#else

    result = "Unknown";

#endif

    if (result.empty())
        result = "Unknown";

    return result;
}

void clearTerminal() {
#if defined(_WIN32) || defined(_WIN64)
    std::system("cls");
#else
    std::system("clear");
#endif
}

void readUserInput() {
    std::cout << "NETSPECTRE@" << serialNumber;
    if (dev_mode) std::cout << "[DEV]";
    else if (pro_basic) std::cout << "[PRO]";
    std::cout << ">>> ";
    std::getline(std::cin, inputLine);
}

// ===================== Command Handling =====================
static void help() {
    std::cout << "\nCommands:\n";
    std::cout << "  info                - Version/Serial\n";
    std::cout << "  cls | clear          - clear terminal\n";
    std::cout << "  cls -l | clear -l    - clear + redraw logo\n";
    std::cout << "  record              - AkteViewer interactive mode\n";
    std::cout << "  record new          - create and open new record\n";
    std::cout << "  record open <Name>  - open <Name>.persondata\n";
    std::cout << "  help                - show command list\n";
    std::cout << "  exit                - quit program\n";
    std::cout << "\nPro Basic Commands:\n";
    std::cout << "  proinfo             - show Pro Basic account info\n";
    std::cout << "  prostatus           - show Pro Basic feature status\n";
    std::cout << "\nDev Commands:\n";
    std::cout << "  devinfo             - show dev status\n";
    std::cout << "  devmode on|off      - toggle (DEV only)\n\n";
    std::cout << "  devtools            - list developer tool commands\n\n";
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

void processInput() {
    std::string cmd = trim(inputLine);

    if (cmd == "info") {
        std::cout << "NetSpectre Pro Terminal C++ edition\n";
        std::cout << "V." << version << "\n";
        std::cout << "Serial number: " << serialNumber << "\n";
        std::cout << "Dev mode: " << (dev_mode ? "true" : "false") << "\n";
    }
    else if (cmd == "cls" || cmd == "clear") {
        clearTerminal();
    }
    else if (cmd == "cls -l" || cmd == "clear -l") {
        clearTerminal();
        showMainScreen();
    }
    else if (cmd == "help") {
        help();
    }
    else if (cmd == "record") {
        openRecordInteractive();
        clearTerminal();
        showMainScreen();
    }
    else if (cmd == "record new") {
        std::string fileName = createNewRecord();
        openRecordFile(fileName);
        clearTerminal();
        showMainScreen();
    }
    else if (starts_with(cmd, "record open ")) {
        std::string name = trim(cmd.substr(std::string("record open ").size()));
        if (name.empty()) {
            std::cout << "ERROR: Please provide a file name.\n";
            return;
        }
        openRecordFile(name);
        clearTerminal();
        showMainScreen();
    }
    else if (cmd == "devinfo") {
        std::cout << "Developer mode is " << (dev_mode ? "ON" : "OFF") << "\n";
        std::cout << "User: " << username << "\n";
    }
    else if (cmd == "proinfo") {
        if (!pro_basic && !developer) {
            std::cout << "Access denied (PRO BASIC or DEV only).\n";
            return;
        }
        std::cout << "Pro Basic account active: " << ((pro_basic || developer) ? "true" : "false") << "\n";
        std::cout << "User: " << username << "\n";
    }
    else if (cmd == "prostatus") {
        if (!pro_basic && !developer) {
            std::cout << "Access denied (PRO BASIC or DEV only).\n";
            return;
        }
        std::cout << "[PRO BASIC] Features available:\n";
        std::cout << "- Priority account tier\n";
        std::cout << "- Extended command set (proinfo/prostatus)\n";
        std::cout << "- Record access\n";
    }
    else if (starts_with(cmd, "devmode ")) {
        std::string arg = to_lower(trim(cmd.substr(std::string("devmode ").size())));
        if (!developer) {
            std::cout << "Access denied (DEV only).\n";
            return;
        }
        if (arg == "on") dev_mode = true;
        else if (arg == "off") dev_mode = false;
        else std::cout << "Usage: devmode on|off\n";
    }
    else if (cmd == "devtools") {
        if (!developer) {
            std::cout << "Access denied (DEV only).\n";
            return;
        }
        std::cout << "[DEV TOOLS]\n";
        std::cout << "- devinfo\n";
        std::cout << "- devmode on|off\n";
        std::cout << "- devtools\n";
    }
    else if (cmd == "exit") {
        // main handles exit
    }
    else {
        std::cout << "Command not found :(\n";
        std::cout << "Tip: 'help'\n";
    }
}

// ===================== Login =====================
void initializeTokens() {
    // Regular users
    userTokens.emplace_back("Laurenz.Flecki66");
    userTokens.emplace_back("Hendrik.Hoppel10");
    userTokens.emplace_back("Joerg.hamburg");

    // Pro Basic users
    proBasicTokens.emplace_back("Hendrik_pro.Hoppel10");
    proBasicTokens.emplace_back("Laura_pro.Basic42");

    // Developers
    devTokens.emplace_back("Hendrik_dev.Hoppel10");
}

void login() {
    logo();
    std::cout << "\nUsername: ";
    std::getline(std::cin, username);
    clearTerminal();

    logo();
    std::cout << "\nPassword for " << username << ": ";
    std::getline(std::cin, password);

    composedToken = username + "." + password;

    if (std::find(devTokens.begin(), devTokens.end(), composedToken) != devTokens.end()) {
        dev_mode = true;
        developer = true;
        pro_basic = true;
        clearTerminal();
        std::cout << "[Developer Mode]\n";
        showMainScreen();
        return;
    }

    if (std::find(proBasicTokens.begin(), proBasicTokens.end(), composedToken) != proBasicTokens.end()) {
        dev_mode = false;
        developer = false;
        pro_basic = true;
        clearTerminal();
        std::cout << "[Pro Basic Account]\n";
        showMainScreen();
        return;
    }

    if (std::find(userTokens.begin(), userTokens.end(), composedToken) != userTokens.end()) {
        dev_mode = false;
        developer = false;
        pro_basic = false;
        clearTerminal();
        showMainScreen();
        return;
    }

    std::cout << "Username or password not recognized.\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));
    clearTerminal();
    login();
}

void showMainScreen() {
    if (dev_mode) {
        std::cout << "Version: " << version << "\n";
    }
    else if (pro_basic) {
        std::cout << "Welcome to NETSPECTRE PRO C++ [PRO BASIC]\n";
        std::cout << "Version: " << short_version << "\n";
        std::cout << "Feedback: Hendrik.Hanking@icloud.com.\n";
    }
    else {
        std::cout << "Welcome to NETSPECTRE PRO C++\n";
        std::cout << "Version: " << short_version << "\n";
        std::cout << "Feedback: Hendrik.Hanking@icloud.com.\n";
    }
    logo();
    std::cout << "\n";
}

void logo() {
    if (dev_mode) {
        std::cout << R"(   _  __    __    ____             __            ___  ___  ____ )" << "\n";
        std::cout << R"(  / |/ /__ / /_  / __/__  ___ ____/ /________   / _ \/ _ \/ __ \)" << "\n";
        std::cout << R"( /    / -_) __/ _\ \/ _ \/ -_) __/ __/ __/ -_) / ___/ , _/ /_/ /)" << "\n";
        std::cout << R"(/_/|_/\__/\__/ /___/ .__/\__/\__/\__/_/  \__/ /_/  /_/|_|\____/ )" << "\n";
        std::cout << R"(C++ EDITION       /_/                     Welcome developer! :) )" << "\n";
    }
    else {
        std::cout << R"(   _  __    __    ____             __            ___  ___  ____ )" << "\n";
        std::cout << R"(  / |/ /__ / /_  / __/__  ___ ____/ /________   / _ \/ _ \/ __ \)" << "\n";
        std::cout << R"( /    / -_) __/ _\ \/ _ \/ -_) __/ __/ __/ -_) / ___/ , _/ /_/ /)" << "\n";
        std::cout << R"(/_/|_/\__/\__/ /___/ .__/\__/\__/\__/_/  \__/ /_/  /_/|_|\____/ )" << "\n";
        std::cout << R"(C++ EDITION       /_/                                           )" << "\n";
    }
}
// ===================== AkteViewer - UX =====================
void fakeLoading(double zeit) {
    constexpr int steps = 10;
    double waitSeconds = zeit / static_cast<double>(steps);

    for (int fillSteps = 0; fillSteps <= steps; ++fillSteps) {
        clearTerminal();
        std::cout << std::string(fillSteps, 'X') << std::string(steps - fillSteps, '0') << "\n";
        auto ms = static_cast<int>(waitSeconds * 1000.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

void openRecordInteractive() {
    clearTerminal();
    std::string fileName;
    std::cout << "Which file name should be opened? (new = create new record): ";
    std::getline(std::cin, fileName);
    fileName = trim(fileName);

    if (to_lower(fileName) == "new") {
        fileName = createNewRecord();
    }

    openRecordFile(fileName);
}

void openRecordFile(const std::string& fileName) {
    if (trim(fileName).empty()) {
        std::cout << "ERROR: Empty file name.\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return;
    }

    fakeLoading(1);
    clearTerminal();

    std::string fullname = fileName + ".persondata";
    std::string content_encrypted;

    try {
        content_encrypted = read_file_utf8(fullname);
    } catch (...) {
        std::cout << "ERROR: File not found: " << fullname << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return;
    }

    std::string content_plain = decryptText(content_encrypted);

    clearTerminal();
    std::cout << content_plain << "\n";
    std::cout << "---- New entries (empty line = save & exit) ----\n";

    while (true) {
        std::string newEntry;
        std::cout << "> ";
        std::getline(std::cin, newEntry);

        if (trim(newEntry).empty()) {
            fakeLoading(1);

            std::string verschluesselt_text = encryptText(content_plain);
            try {
                write_file_utf8(fullname, verschluesselt_text);
            } catch (...) {
                std::cout << "ERROR: Could not save file.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                return;
            }

            clearTerminal();
            std::cout << "Saved.\n";
            std::cout << "ENTER...";
            std::string dummy;
            std::getline(std::cin, dummy);
            return;
        }

        content_plain += "\n" + newEntry;

        clearTerminal();
        std::cout << content_plain << "\n";
        std::cout << "---- New entries (empty line = save & exit) ----\n";
    }
}

std::string createNewRecord() {
    while (true) {
        clearTerminal();
        std::string firstName, name;

        std::cout << "First name: ";
        std::getline(std::cin, firstName);
        firstName = trim(firstName);

        std::cout << "Last name: ";
        std::getline(std::cin, name);
        name = trim(name);

        if (name.empty()) {
            std::cout << "ERROR: Last name cannot be empty\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        std::string fullName = trim(firstName + " " + name);
        std::string fileName = remove_spaces(fullName);
        std::string fullname = fileName + ".persondata";

        std::string key;
        std::cout << "Enter KEY (empty = random key): ";
        std::getline(std::cin, key);
        key = trim(key);

        if (key.empty()) {
            std::string initials = buildInitials(fullName);
            long long number = random_int64(1000000000LL, 99999999999LL);
            std::string chars = to_lower(fileName);
            std::string shuffled = shuffle_string(chars);
            key = initials + std::to_string(number) + shuffled;
        }

        clearTerminal();
        std::cout << "New key:\n" << key << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::string default_plain;
        default_plain += "----- " + fullName + " -----\n";
        default_plain += "- " + key + " -\n";
        default_plain += "-Last name: " + to_upper(name) + "\n";
        default_plain += "-First name: " + firstName + "\n";

        std::string default_encrypted = encryptText(default_plain);

        clearTerminal();
        std::cout << "Creating file...\n";

        try {
            write_file_utf8(fullname, default_encrypted);
        } catch (...) {
            std::cout << "ERROR: Could not write file: " << fullname << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        fakeLoading(2);
        return fileName;
    }
}

// ===================== Crypto =====================
std::string shiftLetters(const std::string& input_text,const std::vector<int>& shifts) {
    std::string encrypted;
    encrypted.reserve(input_text.size());
    size_t shiftIndex = 0;

    for (unsigned char uc : input_text) {
        char letter = static_cast<char>(uc);

        if (std::isalpha(uc)) {
            char base = std::isupper(uc) ? 'A' : 'a';
            int shift = shifts[shiftIndex % shifts.size()];

            char newLetter = static_cast<char>(
                ((letter - base + shift) % 26) + base
            );

            encrypted.push_back(newLetter);
            ++shiftIndex;
        } else {
            encrypted.push_back(letter);
        }
    }
    return encrypted;
}

std::string unshiftLetters(const std::string& text,const std::vector<int>& shifts) {
    std::string decrypted;
    decrypted.reserve(text.size());
    size_t shiftIndex = 0;

    for (unsigned char uc : text) {
        char letter = static_cast<char>(uc);

        if (std::isalpha(uc)) {
            char base = std::isupper(uc) ? 'A' : 'a';
            int shift = shifts[shiftIndex % shifts.size()];

            char newLetter = static_cast<char>(
                ((letter - base - shift + 26) % 26) + base
            );

            decrypted.push_back(newLetter);
            ++shiftIndex;
        } else {
            decrypted.push_back(letter);
        }
    }
    return decrypted;
}

std::string encryptText(const std::string& text) {
    return shiftLetters(text, SHIFT);
}

std::string decryptText(const std::string& text) {
    return unshiftLetters(text, SHIFT);
}

// ===================== File Utils =====================
std::string read_file_utf8(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("not found");
    std::string data((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    return data;
}

void write_file_utf8(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write");
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) throw std::runtime_error("write failed");
}

nlohmann::json readJsonFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open JSON file: " + path);
    }

    nlohmann::json document;
    try {
        input >> document;
    } catch (const nlohmann::json::parse_error& error) {
        throw std::runtime_error("Invalid JSON in " + path + ": " + error.what());
    }

    if (!document.is_object()) {
        throw std::runtime_error("JSON config must contain an object: " + path);
    }

    return document;
}

void writeJsonFile(const std::string& path, const nlohmann::json& document) {
    if (!document.is_object()) {
        throw std::runtime_error("JSON config must be an object: " + path);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot write JSON file: " + path);
    }

    output << document.dump(4) << '\n';
    output.flush();
    if (!output) {
        throw std::runtime_error("Failed while writing JSON file: " + path);
    }
}

void createJsonFile(const std::string& path, const nlohmann::json& initialDocument) {
    if (!initialDocument.is_object()) {
        throw std::runtime_error("Initial JSON config must be an object: " + path);
    }

    std::ofstream output(path, std::ios::binary | std::ios::app);
    if (!output) {
        throw std::runtime_error("Cannot create JSON file: " + path);
    }

    if (output.tellp() > 0) {
        return;
    }

    output << initialDocument.dump(4) << '\n';
    output.flush();
    if (!output) {
        throw std::runtime_error("Failed while creating JSON file: " + path);
    }
}

// ===================== String / Random Utils =====================
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string to_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string remove_spaces(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char uc : s) {
        if (!std::isspace(uc)) out.push_back(static_cast<char>(uc));
    }
    return out;
}

std::mt19937& rng() {
    static std::mt19937 gen(
        static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        )
    );
    return gen;
}

long long random_int64(long long lo, long long hi) {
    std::uniform_int_distribution<long long> dist(lo, hi);
    return dist(rng());
}

std::string shuffle_string(std::string s) {
    std::shuffle(s.begin(), s.end(), rng());
    return s;
}

std::string buildInitials(const std::string& fullName) {
    std::string initials;
    bool newWord = true;
    for (unsigned char uc : fullName) {
        if (std::isspace(uc)) {
            newWord = true;
        } else if (newWord) {
            initials.push_back(static_cast<char>(std::toupper(uc)));
            newWord = false;
        }
    }
    return initials;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}
