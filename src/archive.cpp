#include "archive.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

#include "proc.h"

TempDir::TempDir() {
    std::string tmpl = (fs::temp_directory_path() / "autograder-XXXXXX").string();
    if (!mkdtemp(tmpl.data())) throw std::runtime_error("could not create temp directory");
    path_ = tmpl;
}

TempDir::~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
}

bool isZip(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext == ".zip";
}

void extractZip(const fs::path& zip, const fs::path& dest) {
    fs::create_directories(dest);
    auto r = runProcess({"unzip", "-q", "-o", zip.string(), "-d", dest.string()});
    // unzip exits 1 for warnings (e.g. odd filenames) but still extracts.
    if (r.exitCode != 0 && r.exitCode != 1)
        throw std::runtime_error("unzip failed for " + zip.string() + ":\n" + r.output);
}

void createZip(const fs::path& zip, const std::vector<fs::path>& files) {
    std::error_code ec;
    fs::remove(zip, ec);
    std::vector<std::string> argv = {"zip", "-q", "-j", zip.string()};
    for (const auto& f : files) argv.push_back(f.string());
    auto r = runProcess(argv);
    if (r.exitCode != 0) throw std::runtime_error("zip failed:\n" + r.output);
}
