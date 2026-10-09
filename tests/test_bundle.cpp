#undef NDEBUG  // keep asserts in Release builds
#include <cassert>
#include <fstream>
#include <iostream>

#include "archive.h"
#include "bundle.h"
#include "collect.h"
#include "proc.h"

using V = std::vector<std::string>;

static bool throws(const V& files) {
    try {
        sortBundle(files);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    // Wrapper folder "hw5/" is ignored, so its name doesn't make everything homework.
    BundlePlan p = sortBundle({"hw5/HW5_answers.py", "hw5/Rubric.pdf", "hw5/spec.txt", "hw5/data/input.csv"});
    assert(p.rubric == V{"hw5/Rubric.pdf"});
    assert(p.homework == V{"hw5/HW5_answers.py"});
    assert((p.context == V{"hw5/spec.txt", "hw5/data/input.csv"}));

    // A homework folder takes everything in it; "rubric" beats "hw".
    p = sortBundle({"hw3-rubric.md", "Homework3/main.c", "Homework3/util.h", "hwk3.zip", "starter.c"});
    assert(p.rubric == V{"hw3-rubric.md"});
    assert((p.homework == V{"Homework3/main.c", "Homework3/util.h", "hwk3.zip"}));
    assert(p.context == V{"starter.c"});

    // hw must be its own word: not inside "show" or "schwab".
    p = sortBundle({"rubric.txt", "showcase.py", "schwab.txt", "hw.py"});
    assert(p.homework == V{"hw.py"});
    assert(p.context.size() == 2);

    assert(throws({"hw1.py", "spec.txt"}));      // no rubric
    assert(throws({"rubric.pdf", "answers.py"}));  // no homework

    // bundleFiles skips OS junk.
    TempDir tmp;
    auto write = [&](const std::string& rel) {
        fs::create_directories((tmp.path() / rel).parent_path());
        std::ofstream(tmp.path() / rel) << "x";
    };
    write("hw5/a.py");
    write("__MACOSX/hw5/._a.py");
    write("hw5/.DS_Store");
    assert(bundleFiles(tmp.path()) == V{"hw5/a.py"});

    // Symbolic links are never followed: not listed, not read, and zips holding
    // them are refused.
    fs::path secret = tmp.path() / "secret.txt";
    std::ofstream(secret) << "api_key=sk-secret";
    fs::create_symlink(secret, tmp.path() / "hw5" / "hw5_link.txt");
    fs::create_directory_symlink(tmp.path(), tmp.path() / "hw5" / "hw5_dirlink");
    assert(bundleFiles(tmp.path() / "hw5") == V{"a.py"});

    Collected c;
    size_t budget = 100000;
    collectPath(tmp.path() / "hw5", "hw5", CollectLimits{}, budget, c);
    assert(c.text.find("sk-secret") == std::string::npos);

    fs::path zip = tmp.path() / "evil.zip";
    auto r = runProcess({"sh", "-c", "cd '" + (tmp.path() / "hw5").string() + "' && zip -qy '" + zip.string() +
                                         "' a.py hw5_link.txt"});
    assert(r.exitCode == 0);
    assert(zipSymlinks(zip) == V{"hw5_link.txt"});
    bool refused = false;
    try {
        extractZip(zip, tmp.path() / "out");
    } catch (const std::runtime_error& e) {
        refused = std::string(e.what()).find("hw5_link.txt") != std::string::npos;
    }
    assert(refused && !fs::exists(tmp.path() / "out"));

    // Copying a received folder skips links instead of following them.
    fs::path copied = tmp.path() / "copied";
    fs::copy(tmp.path() / "hw5", copied, fs::copy_options::recursive | fs::copy_options::skip_symlinks);
    assert(!fs::exists(copied / "hw5_link.txt"));

    std::cout << "test_bundle: OK\n";
    return 0;
}
