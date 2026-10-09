#undef NDEBUG  // keep asserts in Release builds
#include <cassert>
#include <iostream>

#include "recurring.h"

int main() {
    std::vector<RecurringEntry> entries;

    // First run: two new errors.
    auto out = mergeRecurring(entries,
                              {{"Off-by-one loop bounds", "Logic", "Loops run one too far", ""},
                               {"Missing input validation", "Robustness", "No checks | on args", ""}},
                              "hw1", "2026-10-01");
    assert(entries.size() == 2);
    assert(out.size() == 2 && !out[0].repeat && out[0].weight == 1);

    // Round-trip through markdown, including an escaped pipe.
    std::string md = renderRecurring(entries);
    entries = parseRecurring(md);
    assert(entries.size() == 2);
    assert(entries[1].description == "No checks | on args" || entries[0].description == "No checks | on args");

    // Second run: repeat via existing_match, repeat via case-insensitive name, and a
    // duplicate mapping to the same entry, which must only count once.
    out = mergeRecurring(entries,
                         {{"Loop bound errors", "Logic", "", "Off-by-one loop bounds"},
                          {"missing INPUT validation", "Robustness", "", ""},
                          {"Fencepost error", "Logic", "", "Off-by-one loop bounds"},
                          {"Unlabeled plot axes", "Presentation", "Axes lack labels", ""}},
                         "hw2, part A", "2026-10-08");
    assert(entries.size() == 3);
    assert(out.size() == 3);
    for (const auto& e : entries) {
        if (e.name == "Off-by-one loop bounds") {
            assert(e.weight == 2);
            assert(e.firstSeen == "2026-10-01" && e.lastSeen == "2026-10-08");
            assert(e.assignments.size() == 2 && e.assignments[1] == "hw2; part A");
        }
        if (e.name == "Missing input validation") assert(e.weight == 2);
        if (e.name == "Unlabeled plot axes") assert(e.weight == 1);
    }

    // Same assignment graded again: weight goes up, assignment isn't duplicated.
    entries = parseRecurring(renderRecurring(entries));
    mergeRecurring(entries, {{"Off-by-one loop bounds", "Logic", "", ""}}, "hw2, part A", "2026-10-09");
    md = renderRecurring(entries);
    entries = parseRecurring(md);
    assert(entries[0].name == "Off-by-one loop bounds");  // sorted by weight
    assert(entries[0].weight == 3);
    assert(entries[0].assignments.size() == 2);

    assert(parseRecurring("").empty());
    std::cout << md << "\ntest_recurring: OK\n";
    return 0;
}
