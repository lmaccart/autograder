#include "archive.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
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

std::vector<std::string> zipSymlinks(const fs::path& zip) {
    // zipinfo lines look like: "lrwxr-xr-x  3.0 unx  10 bx stor 26-Oct-09 14:36 name"
    auto r = runProcess({"unzip", "-Z", zip.string()});
    std::vector<std::string> links;
    std::istringstream lines(r.output);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] != 'l') continue;
        std::istringstream fields(line);
        std::string skip;
        for (int i = 0; i < 8 && fields >> skip;) ++i;
        std::string name;
        std::getline(fields >> std::ws, name);
        links.push_back(name);
    }
    return links;
}

void removeSymlinks(const fs::path& dir) {
    std::vector<fs::path> links;
    for (const auto& e : fs::recursive_directory_iterator(dir))  // doesn't follow directory links
        if (e.is_symlink()) links.push_back(e.path());
    for (const auto& l : links) fs::remove(l);
}

void extractZip(const fs::path& zip, const fs::path& dest) {
    // A link in the zip could point anywhere on this machine (e.g. at the API key),
    // so links are refused outright rather than extracted.
    auto links = zipSymlinks(zip);
    if (!links.empty()) {
        std::string names;
        for (const auto& l : links) names += "\n  " + l;
        throw std::runtime_error(zip.filename().string() +
                                 " contains symbolic links, which aren't allowed:" + names);
    }
    fs::create_directories(dest);
    auto r = runProcess({"unzip", "-q", "-o", zip.string(), "-d", dest.string()});
    // unzip exits 1 for warnings (e.g. odd filenames) but still extracts.
    if (r.exitCode != 0 && r.exitCode != 1)
        throw std::runtime_error("unzip failed for " + zip.string() + ":\n" + r.output);
    removeSymlinks(dest);  // belt and braces, in case the listing missed one
}

void createZip(const fs::path& zip, const std::vector<fs::path>& files) {
    std::error_code ec;
    fs::remove(zip, ec);
    std::vector<std::string> argv = {"zip", "-q", "-j", zip.string()};
    for (const auto& f : files) argv.push_back(f.string());
    auto r = runProcess(argv);
    if (r.exitCode != 0) throw std::runtime_error("zip failed:\n" + r.output);
}
