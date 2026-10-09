#pragma once
#include <string>
#include <vector>

struct RecurringEntry {
    std::string name;
    std::string category;
    int weight = 0;
    std::string firstSeen;
    std::string lastSeen;
    std::vector<std::string> assignments;
    std::string description;
};

// An error reported by the model for this run.
struct FoundError {
    std::string name;
    std::string category;
    std::string description;
    std::string existingMatch;  // name of an existing entry, or empty
};

struct MergeOutcome {
    std::string name;
    int weight = 0;
    bool repeat = false;  // true if it was already in RECURRING.md
};

std::vector<RecurringEntry> parseRecurring(const std::string& markdown);
std::string renderRecurring(std::vector<RecurringEntry> entries);

// Adds this run's errors to `entries`. An entry's weight goes up by at most
// one per run, even if several reported errors map to it.
std::vector<MergeOutcome> mergeRecurring(std::vector<RecurringEntry>& entries,
                                         const std::vector<FoundError>& found,
                                         const std::string& assignment, const std::string& date);
