#pragma once
#include "config.h"

// Grades cfg.solution against cfg.rubric, writes outputs, updates
// RECURRING.md and Taildrops the results. Returns the process exit code.
// Throws ApiError for API failures and std::exception for anything else.
int runGrading(const Config& cfg);

// Local time formatted with strftime.
std::string now(const char* fmt);

// `s` with anything but [A-Za-z0-9._-] replaced by '_', for file names.
std::string safeName(const std::string& s);
