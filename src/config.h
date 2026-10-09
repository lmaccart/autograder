#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

struct Config {
    fs::path rubric;
    fs::path files;
    fs::path solution;
    std::string name;
    std::string model = "gpt-6.1-sol";
    fs::path bundle;       // a zip or folder to sort by file name, then grade
    bool nameSet = false;  // --name given explicitly
    std::string apiKey;
    std::string baseUrl = "https://api.openai.com/v1";
    fs::path outDir = "autograder-out";
    bool outSet = false;  // --out or out= given explicitly
    fs::path recurring;
    std::string target = "mbp";
    bool noSend = false;
    bool dryRun = false;
    int maxWaitSeconds = 900;
    size_t maxChars = 300000;     // total inline text budget sent to the model
    size_t maxFileChars = 100000; // per-file cap before truncation

    // Watch mode: grade whatever arrives in `inbox` via Taildrop.
    bool watch = false;
    fs::path inbox;
    int pollSeconds = 5;
    int settleSeconds = 15;  // wait this long after the last new file before grading
};

// Directory for global state: ~/.autograder
fs::path appDir();

// Expands a leading "~/" to $HOME.
fs::path expandHome(const std::string& p);

// Builds the config from defaults, ~/.autograder/config, env, then argv.
// Prints usage and exits on --help or bad arguments.
Config loadConfig(int argc, char** argv);

void printUsage();
