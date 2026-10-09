#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// A temporary directory that is removed when it goes out of scope.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

bool isZip(const fs::path& p);

// Extracts `zip` into `dest`. Throws std::runtime_error on failure.
void extractZip(const fs::path& zip, const fs::path& dest);

// Creates `zip` containing `files` (flat, no directory structure).
// Throws std::runtime_error on failure.
void createZip(const fs::path& zip, const std::vector<fs::path>& files);
