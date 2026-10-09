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
    std::string apiKey;
    std::string baseUrl = "https://api.openai.com/v1";
    fs::path outDir = "autograder-out";
    fs::path recurring;
    std::string target = "mbp";
    bool noSend = false;
    bool dryRun = false;
    int maxWaitSeconds = 900;
    size_t maxChars = 300000;     // total inline text budget sent to the model
    size_t maxFileChars = 100000; // per-file cap before truncation
};

// Directory for global state: ~/.autograder
fs::path appDir();

// Expands a leading "~/" to $HOME.
fs::path expandHome(const std::string& p);

// Builds the config from defaults, ~/.autograder/config, env, then argv.
// Prints usage and exits on --help or bad arguments.
Config loadConfig(int argc, char** argv);

void printUsage();
