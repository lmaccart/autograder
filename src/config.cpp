#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

fs::path appDir() {
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".autograder";
}

fs::path expandHome(const std::string& p) {
    if (p.rfind("~/", 0) == 0) {
        const char* home = std::getenv("HOME");
        if (home) return fs::path(home) / p.substr(2);
    }
    return p;
}

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

void printUsage() {
    std::cerr <<
R"(Usage: autograder --rubric FILE --solution ZIP|FILE [--files ZIP] [options]

Grades your submission against a rubric with the OpenAI API, writes TODO.md,
STUDY.md and SUMMARY.md, updates RECURRING.md, and Taildrops the results.

Required:
  --rubric FILE        Rubric (text, markdown, or PDF)
  --solution PATH      Your submission: a zip or a single file

Options:
  --files ZIP          Zip of context files (assignment spec, starter code, ...)
  --name NAME          Assignment label (default: solution file name)
  --model MODEL        OpenAI model (default: gpt-6.1-sol)
  --out DIR            Output directory (default: ./autograder-out)
  --recurring FILE     RECURRING.md path (default: ~/.autograder/RECURRING.md)
  --target DEVICE      Taildrop target (default: mbp)
  --no-send            Don't Taildrop the results
  --dry-run            Show what would be sent; don't call the API
  --max-wait SECONDS   Give up retrying rate limits after this long (default: 900)
  --max-chars N        Total characters of file text to send (default: 300000)
  -h, --help           Show this help

The API key comes from $OPENAI_API_KEY, or api_key= in ~/.autograder/config.
That file may also set model=, target=, recurring=, out=, max_wait=, base_url=.
)";
}

static void applyConfigFile(Config& c, const fs::path& path) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "api_key") c.apiKey = val;
        else if (key == "model") c.model = val;
        else if (key == "target") c.target = val;
        else if (key == "recurring") c.recurring = expandHome(val);
        else if (key == "out") c.outDir = expandHome(val);
        else if (key == "base_url") c.baseUrl = val;
        else if (key == "max_wait") c.maxWaitSeconds = std::stoi(val);
    }
}

[[noreturn]] static void usageError(const std::string& msg) {
    std::cerr << "error: " << msg << "\n\n";
    printUsage();
    std::exit(1);
}

Config loadConfig(int argc, char** argv) {
    Config c;
    c.recurring = appDir() / "RECURRING.md";
    applyConfigFile(c, appDir() / "config");
    if (const char* k = std::getenv("OPENAI_API_KEY"); k && *k) c.apiKey = k;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) usageError(a + " needs a value");
            return argv[++i];
        };
        try {
            if (a == "-h" || a == "--help") { printUsage(); std::exit(0); }
            else if (a == "--rubric") c.rubric = expandHome(next());
            else if (a == "--files") c.files = expandHome(next());
            else if (a == "--solution") c.solution = expandHome(next());
            else if (a == "--name") c.name = next();
            else if (a == "--model") c.model = next();
            else if (a == "--out") c.outDir = expandHome(next());
            else if (a == "--recurring") c.recurring = expandHome(next());
            else if (a == "--target") c.target = next();
            else if (a == "--no-send") c.noSend = true;
            else if (a == "--dry-run") c.dryRun = true;
            else if (a == "--max-wait") c.maxWaitSeconds = std::stoi(next());
            else if (a == "--max-chars") c.maxChars = std::stoul(next());
            else usageError("unknown argument: " + a);
        } catch (const std::logic_error&) {
            usageError("bad number for " + a);
        }
    }

    if (c.rubric.empty() || c.solution.empty())
        usageError("--rubric and --solution are required");
    for (const auto& p : {c.rubric, c.files, c.solution})
        if (!p.empty() && !fs::exists(p)) usageError("not found: " + p.string());
    if (c.name.empty()) c.name = c.solution.stem().string();
    if (!c.dryRun && c.apiKey.empty())
        usageError("no API key: set OPENAI_API_KEY or api_key= in " + (appDir() / "config").string());
    return c;
}
