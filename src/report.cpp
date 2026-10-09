#include "report.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// Indents continuation lines so multi-line text stays inside its bullet.
std::string indent(const std::string& text, const std::string& pad) {
    std::string out;
    for (char c : text) {
        out += c;
        if (c == '\n') out += pad;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
    return out;
}

}  // namespace

std::string renderTodo(const std::string& assignment, const GradeResult& r) {
    std::ostringstream o;
    o << "# TODO: " << assignment << "\n\n";
    if (r.todo.empty()) {
        o << "Nothing to change. Nice work.\n";
        return o.str();
    }
    o << "Ordered by points at stake (highest first).\n\n";
    for (const auto& t : r.todo) {
        o << "- [ ] **" << indent(t.title, "  ") << "**\n";
        if (!t.rubricItem.empty()) o << "  - **Rubric:** " << indent(t.rubricItem, "    ") << "\n";
        if (!t.location.empty()) o << "  - **Where:** " << indent(t.location, "    ") << "\n";
        o << "  - **Problem:** " << indent(t.problem, "    ") << "\n";
        o << "  - **Fix:** " << indent(t.fix, "    ") << "\n\n";
    }
    return o.str();
}

std::string renderStudy(const std::string& assignment, const GradeResult& r) {
    std::ostringstream o;
    o << "# Study & Improve: " << assignment << "\n\n";
    if (r.study.empty()) {
        o << "No specific study topics this time.\n";
        return o.str();
    }
    for (const auto& s : r.study) {
        o << "- **" << indent(s.topic, "  ") << "**\n";
        o << "  - **Why:** " << indent(s.why, "    ") << "\n";
        o << "  - **How:** " << indent(s.how, "    ") << "\n\n";
    }
    return o.str();
}

std::string renderSummary(const std::string& assignment, const std::string& date, const std::string& model,
                          const GradeResult& r, const std::vector<MergeOutcome>& recurring,
                          const std::vector<std::string>& skipped) {
    std::ostringstream o;
    o << "# Summary: " << assignment << "\n\n";
    o << "- **Graded:** " << date << " (model: " << model << ")\n";
    o << "- **Estimated score:** " << indent(r.estimatedScore, "  ") << "\n";
    o << "- **TODO items:** " << r.todo.size() << "\n";
    o << "- **Study topics:** " << r.study.size() << "\n\n";
    o << "## Assessment\n\n" << r.summary << "\n\n";

    o << "## Errors this time\n\n";
    if (recurring.empty()) o << "None recorded.\n";
    for (const auto& m : recurring) {
        o << "- " << m.name;
        if (m.repeat) o << " — **REPEAT** (weight now " << m.weight << ")";
        else o << " — new";
        o << "\n";
    }

    if (!skipped.empty()) {
        o << "\n## Files not fully sent to the model\n\n";
        for (const auto& s : skipped) o << "- " << s << "\n";
    }
    return o.str();
}

void writeFileAtomic(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out << content;
        if (!out) throw std::runtime_error("failed writing " + tmp.string());
    }
    fs::rename(tmp, path);
}
