#undef NDEBUG  // keep asserts in Release builds
#include <cassert>
#include <fstream>
#include <iostream>

#include "archive.h"
#include "bundle.h"

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

    std::cout << "test_bundle: OK\n";
    return 0;
}
