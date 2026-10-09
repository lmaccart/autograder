#undef NDEBUG  // keep asserts in Release builds
#include <cassert>
#include <fstream>
#include <iostream>

#include "archive.h"
#include "watch.h"

int main() {
    TempDir tmp;
    const fs::path& in = tmp.path();
    auto touch = [&](const fs::path& p) {
        fs::create_directories((in / p).parent_path());
        std::ofstream(in / p) << "x";
    };

    assert(groupInbox(in).empty());

    touch("hw3.zip");
    touch("lab2/main.c");
    touch("lab2/rubric.md");
    touch("rubric.pdf");
    touch("answers.py");
    touch(".DS_Store");
    fs::create_directories(in / "__MACOSX");

    auto groups = groupInbox(in);
    assert(groups.size() == 3);  // hw3.zip, lab2/, and the two loose files
    assert(groups[0].size() == 1 && groups[0][0].filename() == "hw3.zip");
    assert(groups[1].size() == 1 && groups[1][0].filename() == "lab2");
    assert(groups[2].size() == 2);
    assert(groups[2][0].filename() == "answers.py" && groups[2][1].filename() == "rubric.pdf");

    std::cout << "test_watch: OK\n";
    return 0;
}
