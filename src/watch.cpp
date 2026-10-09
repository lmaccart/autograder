#include "watch.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <thread>

#include "archive.h"
#include "bundle.h"
#include "collect.h"
#include "pipeline.h"
#include "proc.h"
#include "report.h"
#include "taildrop.h"

namespace {

using Clock = std::chrono::steady_clock;
using Snapshot = std::map<std::string, std::uintmax_t>;  // relative path -> size

std::string stamp() { return "[" + now("%H:%M:%S") + "] "; }

bool visible(const fs::path& p) {
    std::string n = p.filename().string();
    return !n.empty() && n[0] != '.' && !ignoredPath(p.filename());
}

// Every file under the inbox with its size, so a folder still arriving
// counts as a change.
Snapshot snapshot(const fs::path& inbox) {
    Snapshot s;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(inbox, ec); it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file(ec)) s[fs::relative(it->path(), inbox).string()] = it->file_size(ec);
    }
    return s;
}

// Tells you on the Mac that a bundle failed, so you aren't left waiting.
void reportFailure(const Config& cfg, const std::string& what, const std::string& error) {
    fs::path file = cfg.outDir / ("ERROR-" + safeName(what) + "-" + now("%Y%m%d-%H%M%S") + ".md");
    writeFileAtomic(file, "# autograder couldn't grade " + what + "\n\n" + error +
                              "\n\nThe received files are kept in ~/.autograder/jobs/ on the Pi.\n");
    if (cfg.noSend) return;
    std::string err = taildrop(file, cfg.target);
    if (!err.empty()) std::cerr << stamp() << "Couldn't Taildrop the error report either: " << err << "\n";
}

void processBundle(const Config& cfg, const std::vector<fs::path>& items) {
    std::string what = items.size() == 1 ? items[0].filename().string() : std::to_string(items.size()) + " loose files";
    std::cerr << stamp() << "Grading " << what << "\n";
    try {
        int rc = gradeBundle(cfg, items, true);
        std::cerr << stamp() << (rc == 0 ? "Done: " : "Finished with problems: ") << what << "\n";
    } catch (const std::exception& e) {
        std::cerr << stamp() << "Failed: " << what << ": " << e.what() << "\n";
        try {
            reportFailure(cfg, what, e.what());
        } catch (const std::exception& e2) {
            std::cerr << stamp() << "Couldn't write the error report: " << e2.what() << "\n";
        }
    }
}

}  // namespace

std::vector<std::vector<fs::path>> groupInbox(const fs::path& inbox) {
    std::vector<fs::path> entries;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(inbox, ec))
        if (visible(e.path())) entries.push_back(e.path());
    std::sort(entries.begin(), entries.end());

    std::vector<std::vector<fs::path>> groups;
    std::vector<fs::path> loose;
    for (const auto& e : entries) {
        if (fs::is_directory(e) || isZip(e)) groups.push_back({e});
        else if (fs::is_regular_file(e)) loose.push_back(e);
    }
    if (!loose.empty()) groups.push_back(loose);
    return groups;
}

int watchInbox(const Config& cfg) {
    if (!onPath("tailscale")) {
        std::cerr << "error: watch mode needs 'tailscale' installed\n";
        return 1;
    }
    fs::create_directories(cfg.inbox);
    std::cerr << stamp() << "Watching for Taildropped zips or folders in " << cfg.inbox.string()
              << "\n  results: " << cfg.outDir.string() << "  (Ctrl-C to stop)\n";

    Snapshot last;
    auto lastChange = Clock::now();
    std::string lastError;

    for (;;) {
        auto r = runProcess({"tailscale", "file", "get", "--conflict=rename", cfg.inbox.string()});
        std::string err = r.exitCode == 0 ? "" : r.output;
        if (!err.empty() && err != lastError) std::cerr << stamp() << "tailscale file get: " << err << "\n";
        lastError = err;

        Snapshot snap = snapshot(cfg.inbox);
        if (snap != last) {
            if (!snap.empty())
                std::cerr << stamp() << "Receiving (" << snap.size() << " file(s)); grading once nothing new arrives for "
                          << cfg.settleSeconds << "s\n";
            last = snap;
            lastChange = Clock::now();
        }

        auto quiet = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - lastChange).count();
        if (!snap.empty() && quiet >= cfg.settleSeconds) {
            for (const auto& group : groupInbox(cfg.inbox)) processBundle(cfg, group);
            last = snapshot(cfg.inbox);
            std::cerr << stamp() << "Waiting for the next bundle...\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.pollSeconds));
    }
}
