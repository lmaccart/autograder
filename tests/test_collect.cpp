#undef NDEBUG  // keep asserts in Release builds
#include <cassert>
#include <fstream>
#include <iostream>

#include "archive.h"
#include "collect.h"
#include "openai.h"

static void write(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << data;
}

int main() {
    assert(base64Encode("") == "");
    assert(base64Encode("f") == "Zg==");
    assert(base64Encode("fo") == "Zm8=");
    assert(base64Encode("foo") == "Zm9v");
    assert(base64Encode("foobar") == "Zm9vYmFy");

    assert(parseDuration("1s") == 1);
    assert(parseDuration("6m0s") == 360);
    assert(parseDuration("250ms") == 0.25);
    assert(parseDuration("20") == 20);
    assert(parseDuration("1h2m3s") == 3723);
    assert(parseDuration("soon") == -1);

    TempDir tmp;
    fs::path root = tmp.path() / "sub";
    write(root / "main.cpp", "int main() {}\n");
    write(root / "big.txt", std::string(50, 'x'));
    write(root / "img.png", std::string("\x89PNG\0\0data", 11));
    write(root / "__MACOSX" / "junk.txt", "ignored");
    write(root / "doc.pdf", "%PDF-1.4 fake");

    CollectLimits limits;
    limits.maxFileChars = 20;
    size_t budget = 1000;
    Collected c;
    collectPath(root, "sub", limits, budget, c);

    assert(c.text.find("===== FILE: sub/main.cpp =====") != std::string::npos);
    assert(c.text.find("sub/big.txt (truncated)") != std::string::npos);
    assert(c.text.find("junk") == std::string::npos);
    assert(c.pdfs.size() == 1 && c.pdfs[0].name == "doc.pdf");
    bool sawBinary = false;
    for (const auto& s : c.skipped) sawBinary |= s == "sub/img.png (binary)";
    assert(sawBinary);

    // Budget exhaustion skips later files.
    budget = 5;
    Collected c2;
    collectPath(root, "sub", limits, budget, c2);
    assert(budget == 0);
    bool sawBudget = false;
    for (const auto& s : c2.skipped) sawBudget |= s.find("over total size budget") != std::string::npos;
    assert(sawBudget);

    std::cout << "test_collect: OK\n";
    return 0;
}
