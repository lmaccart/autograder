#pragma once
#include <string>
#include <vector>

struct ProcResult {
    int exitCode = -1;
    std::string output;  // combined stdout + stderr
};

// Runs argv[0] (looked up on PATH) without a shell, capturing output.
ProcResult runProcess(const std::vector<std::string>& argv);

// True if `name` is an executable somewhere on PATH.
bool onPath(const std::string& name);
