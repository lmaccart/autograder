#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct PdfFile {
    std::string name;
    std::string base64;
};

struct Collected {
    std::string text;                  // all text files, each with a path header
    std::vector<PdfFile> pdfs;         // sent as file attachments
    std::vector<std::string> skipped;  // "path (reason)"
};

struct CollectLimits {
    size_t maxFileChars = 100000;
    size_t maxPdfBytes = 20 * 1024 * 1024;
};

// Collects every file under `root` (or `root` itself if it is a file).
// `display` is the prefix shown in path headers instead of the temp dir path.
// `budget` is decremented by the number of text characters added; once it
// runs out, remaining files are skipped.
void collectPath(const fs::path& root, const std::string& display, const CollectLimits& limits,
                 size_t& budget, Collected& out);

bool looksBinary(const std::string& data);
std::string base64Encode(const std::string& data);
