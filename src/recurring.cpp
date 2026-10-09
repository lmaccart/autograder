#include "recurring.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return trim(s);
}

// Makes text safe for a single table cell.
std::string escapeCell(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '|') out += "\\|";
        else if (c == '\n' || c == '\r') out += ' ';
        else out += c;
    }
    return trim(out);
}

// Splits "| a | b \| c |" into {"a", "b | c"}.
std::vector<std::string> splitRow(const std::string& line) {
    std::vector<std::string> cells;
    std::string cur;
    std::string s = trim(line);
    if (!s.empty() && s.front() == '|') s.erase(0, 1);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '|') {
            cur += '|';
            ++i;
        } else if (s[i] == '|') {
            cells.push_back(trim(cur));
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    if (!trim(cur).empty()) cells.push_back(trim(cur));
    return cells;
}

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

}  // namespace

std::vector<RecurringEntry> parseRecurring(const std::string& markdown) {
    std::vector<RecurringEntry> entries;
    std::stringstream ss(markdown);
    std::string line;
    while (std::getline(ss, line)) {
        if (trim(line).rfind("|", 0) != 0) continue;
        auto cells = splitRow(line);
        if (cells.size() < 7) continue;
        if (cells[0] == "Error" || cells[0].find("---") != std::string::npos) continue;
        RecurringEntry e;
        e.name = cells[0];
        e.category = cells[1];
        try {
            e.weight = std::stoi(cells[2]);
        } catch (...) {
            e.weight = 1;
        }
        e.firstSeen = cells[3];
        e.lastSeen = cells[4];
        e.assignments = splitList(cells[5]);
        e.description = cells[6];
        entries.push_back(e);
    }
    return entries;
}

std::string renderRecurring(std::vector<RecurringEntry> entries) {
    std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if (a.weight != b.weight) return a.weight > b.weight;
        return a.lastSeen > b.lastSeen;
    });

    std::ostringstream out;
    out << "# Recurring Errors\n\n"
        << "Maintained by autograder. **Weight** = number of graded assignments in which the error\n"
        << "appeared. Highest-weight errors are the ones to focus on first.\n\n"
        << "| Error | Category | Weight | First seen | Last seen | Assignments | Description |\n"
        << "|---|---|---|---|---|---|---|\n";
    for (const auto& e : entries) {
        std::string assignments;
        for (size_t i = 0; i < e.assignments.size(); ++i) {
            if (i) assignments += ", ";
            assignments += e.assignments[i];
        }
        out << "| " << escapeCell(e.name) << " | " << escapeCell(e.category) << " | " << e.weight
            << " | " << escapeCell(e.firstSeen) << " | " << escapeCell(e.lastSeen) << " | "
            << escapeCell(assignments) << " | " << escapeCell(e.description) << " |\n";
    }
    return out.str();
}

std::vector<MergeOutcome> mergeRecurring(std::vector<RecurringEntry>& entries,
                                         const std::vector<FoundError>& found,
                                         const std::string& assignment, const std::string& date) {
    std::vector<MergeOutcome> outcomes;
    std::set<std::string> bumped;  // lowercase names already counted this run
    // Stored as it will appear in the table; commas would split the list on re-read.
    std::string asg = escapeCell(assignment);
    std::replace(asg.begin(), asg.end(), ',', ';');

    auto findEntry = [&](const std::string& name) -> RecurringEntry* {
        std::string key = lower(name);
        if (key.empty()) return nullptr;
        for (auto& e : entries)
            if (lower(e.name) == key) return &e;
        return nullptr;
    };

    for (const auto& f : found) {
        RecurringEntry* e = findEntry(f.existingMatch);
        if (!e) e = findEntry(f.name);
        bool repeat = e != nullptr;

        if (!e) {
            if (trim(f.name).empty()) continue;
            RecurringEntry fresh;
            fresh.name = trim(f.name);
            fresh.category = f.category;
            fresh.firstSeen = date;
            fresh.description = f.description;
            entries.push_back(fresh);
            e = &entries.back();
        }

        std::string key = lower(e->name);
        if (bumped.count(key)) continue;
        bumped.insert(key);

        e->weight += 1;
        e->lastSeen = date;
        if (std::find(e->assignments.begin(), e->assignments.end(), asg) == e->assignments.end())
            e->assignments.push_back(asg);
        outcomes.push_back({e->name, e->weight, repeat});
    }
    return outcomes;
}
