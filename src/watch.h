#pragma once
#include <filesystem>
#include <vector>

#include "config.h"

namespace fs = std::filesystem;

// Splits what's in the inbox into bundles: each zip and each folder is its
// own bundle, and all loose files together make one more.
std::vector<std::vector<fs::path>> groupInbox(const fs::path& inbox);

// Receives files via Taildrop into cfg.inbox forever, grading each bundle
// once nothing new has arrived for cfg.settleSeconds. Only returns on a
// setup error.
int watchInbox(const Config& cfg);
