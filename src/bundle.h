#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

namespace fs = std::filesystem;

// How a bundle's files were split up. Paths are relative to the bundle root.
struct BundlePlan {
    std::vector<std::string> rubric, homework, context;
};

// Every file in the bundle (relative, '/'-separated, sorted), skipping OS junk.
std::vector<std::string> bundleFiles(const fs::path& root);

// Sorts files by name, ignoring a top-level folder that wraps everything:
//   a name containing "rubric"                      -> rubric
//   a file or folder named with hw / hwk / homework -> homework (e.g. hw3.py, HW_3/, homework2.pdf)
//   everything else                                 -> context
// Throws std::runtime_error if there is no rubric or no homework.
BundlePlan sortBundle(const std::vector<std::string>& files);

// Unpacks `items` (one zip, one folder, or loose files) into a job directory
// under ~/.autograder/jobs, sorts the files with sortBundle, then grades.
// With `consume`, the items are moved out of where they are (the inbox).
int gradeBundle(const Config& cfg, const std::vector<fs::path>& items, bool consume);
