#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#include "archive.h"
#include "collect.h"
#include "config.h"
#include "grader.h"
#include "openai.h"
#include "proc.h"
#include "recurring.h"
#include "report.h"
#include "taildrop.h"

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string now(const char* fmt) {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, fmt, std::localtime(&t));
    return buf;
}

std::string safeName(const std::string& s) {
    std::string out;
    for (char c : s) out += (std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') ? c : '_';
    return out.empty() ? "assignment" : out;
}

// Extracts (if a zip) and collects `input` under `label`.
Collected gather(const fs::path& input, const std::string& label, const fs::path& scratch,
                 const CollectLimits& limits, size_t& budget) {
    Collected c;
    if (isZip(input)) {
        fs::path dest = scratch / label;
        extractZip(input, dest);
        collectPath(dest, input.filename().string(), limits, budget, c);
    } else {
        collectPath(input, input.filename().string(), limits, budget, c);
    }
    return c;
}

int run(const Config& cfg) {
    for (const char* tool : {"unzip", "zip"})
        if (!onPath(tool)) throw std::runtime_error(std::string("'") + tool + "' is not installed");
    if (!cfg.noSend && !cfg.dryRun && !onPath("tailscale"))
        throw std::runtime_error("'tailscale' is not installed (use --no-send to skip Taildrop)");

    TempDir scratch;
    CollectLimits limits;
    limits.maxFileChars = cfg.maxFileChars;
    size_t budget = cfg.maxChars;

    // Rubric first so it is never squeezed out by a large context zip.
    GradeInput in;
    in.rubric = gather(cfg.rubric, "rubric", scratch.path(), limits, budget);
    in.submission = gather(cfg.solution, "solution", scratch.path(), limits, budget);
    if (!cfg.files.empty()) in.context = gather(cfg.files, "files", scratch.path(), limits, budget);
    in.known = parseRecurring(readText(cfg.recurring));

    std::vector<std::string> skipped;
    for (const Collected* c : {&in.rubric, &in.submission, &in.context})
        skipped.insert(skipped.end(), c->skipped.begin(), c->skipped.end());
    for (const auto& s : skipped) std::cerr << "note: " << s << "\n";

    nlohmann::json request = buildRequest(cfg.model, in);
    size_t chars = buildPromptText(in).size();
    size_t pdfs = in.rubric.pdfs.size() + in.context.pdfs.size() + in.submission.pdfs.size();
    std::cerr << "Prompt: ~" << chars / 4 << " tokens of text, " << pdfs << " PDF(s), "
              << in.known.size() << " known recurring error(s)\n";

    if (cfg.dryRun) {
        std::cout << buildPromptText(in);
        return 0;
    }

    std::cerr << "Grading with " << cfg.model << " (this can take a few minutes)...\n";
    nlohmann::json response = postResponses(cfg.baseUrl, cfg.apiKey, request, cfg.maxWaitSeconds);

    const std::string date = now("%Y-%m-%d");
    fs::path outDir = cfg.outDir / (safeName(cfg.name) + "-" + now("%Y%m%d-%H%M%S"));
    fs::create_directories(outDir);
    writeFileAtomic(outDir / "response.json", response.dump(2));

    GradeResult result = parseResponse(response);

    // Re-read in case RECURRING.md changed while we waited on the API.
    auto entries = parseRecurring(readText(cfg.recurring));
    auto outcomes = mergeRecurring(entries, result.errors, cfg.name, date);
    std::string recurringMd = renderRecurring(entries);
    writeFileAtomic(cfg.recurring, recurringMd);

    std::vector<fs::path> outputs = {outDir / "TODO.md", outDir / "STUDY.md", outDir / "RECURRING.md",
                                     outDir / "SUMMARY.md"};
    writeFileAtomic(outputs[0], renderTodo(cfg.name, result));
    writeFileAtomic(outputs[1], renderStudy(cfg.name, result));
    writeFileAtomic(outputs[2], recurringMd);
    writeFileAtomic(outputs[3], renderSummary(cfg.name, date, cfg.model, result, outcomes, skipped));

    fs::path zip = outDir / (safeName(cfg.name) + "-feedback.zip");
    createZip(zip, outputs);

    std::cout << "Estimated score: " << result.estimatedScore << "\n"
              << result.todo.size() << " TODO item(s), " << result.study.size() << " study topic(s), "
              << outcomes.size() << " error(s) recorded in " << cfg.recurring.string() << "\n"
              << "Saved to " << outDir.string() << "\n";

    if (cfg.noSend) return 0;
    std::string err = taildrop(zip, cfg.target);
    if (!err.empty()) {
        std::cerr << "Taildrop to " << cfg.target << " failed (files are still saved locally):\n" << err << "\n";
        return 3;
    }
    std::cout << "Sent " << zip.filename().string() << " to " << cfg.target << " via Taildrop\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg = loadConfig(argc, argv);
    try {
        return run(cfg);
    } catch (const ApiError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
