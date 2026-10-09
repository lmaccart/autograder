#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "grader.h"
#include "recurring.h"

namespace fs = std::filesystem;

std::string renderTodo(const std::string& assignment, const GradeResult& r);
std::string renderStudy(const std::string& assignment, const GradeResult& r);
std::string renderSummary(const std::string& assignment, const std::string& date, const std::string& model,
                          const GradeResult& r, const std::vector<MergeOutcome>& recurring,
                          const std::vector<std::string>& skipped);

// Writes `content` to `path` atomically (temp file + rename).
void writeFileAtomic(const fs::path& path, const std::string& content);
