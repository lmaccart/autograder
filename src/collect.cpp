#include "collect.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

static std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool looksBinary(const std::string& data) {
    size_t n = std::min<size_t>(data.size(), 8192);
    return std::find(data.begin(), data.begin() + n, '\0') != data.begin() + n;
}

std::string base64Encode(const std::string& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        unsigned v = (unsigned char)data[i] << 16 | (unsigned char)data[i + 1] << 8 | (unsigned char)data[i + 2];
        out += tbl[v >> 18 & 63];
        out += tbl[v >> 12 & 63];
        out += tbl[v >> 6 & 63];
        out += tbl[v & 63];
    }
    if (i < data.size()) {
        unsigned v = (unsigned char)data[i] << 16;
        if (i + 1 < data.size()) v |= (unsigned char)data[i + 1] << 8;
        out += tbl[v >> 18 & 63];
        out += tbl[v >> 12 & 63];
        out += i + 1 < data.size() ? tbl[v >> 6 & 63] : '=';
        out += '=';
    }
    return out;
}

bool ignoredPath(const fs::path& rel) {
    for (const auto& part : rel) {
        std::string s = part.string();
        if (s == "__MACOSX" || s == ".git" || s == ".DS_Store" || s == "node_modules" ||
            s == "__pycache__" || s == ".idea" || s == ".vscode")
            return true;
    }
    return false;
}

static void collectFile(const fs::path& file, const std::string& shown, const CollectLimits& limits,
                        size_t& budget, Collected& out) {
    if (lower(file.extension().string()) == ".pdf") {
        if (fs::file_size(file) > limits.maxPdfBytes) {
            out.skipped.push_back(shown + " (PDF too large)");
            return;
        }
        out.pdfs.push_back({fs::path(shown).filename().string(), base64Encode(readFile(file))});
        return;
    }

    std::string data = readFile(file);
    if (looksBinary(data)) {
        out.skipped.push_back(shown + " (binary)");
        return;
    }
    if (budget == 0) {
        out.skipped.push_back(shown + " (over total size budget)");
        return;
    }

    bool truncated = false;
    size_t cap = std::min(limits.maxFileChars, budget);
    if (data.size() > cap) {
        data.resize(cap);
        truncated = true;
    }
    budget -= data.size();

    out.text += "===== FILE: " + shown + (truncated ? " (truncated)" : "") + " =====\n";
    out.text += data;
    if (!data.empty() && data.back() != '\n') out.text += '\n';
    out.text += "===== END FILE: " + shown + " =====\n\n";
    if (truncated) out.skipped.push_back(shown + " (truncated)");
}

void collectPath(const fs::path& root, const std::string& display, const CollectLimits& limits,
                 size_t& budget, Collected& out) {
    if (fs::is_regular_file(root)) {
        collectFile(root, display, limits, budget, out);
        return;
    }

    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file()) continue;
        fs::path rel = fs::relative(e.path(), root);
        if (ignoredPath(rel)) continue;
        files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        std::string shown = (fs::path(display) / fs::relative(f, root)).generic_string();
        collectFile(f, shown, limits, budget, out);
    }
}
