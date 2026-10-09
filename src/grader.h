#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "collect.h"
#include "recurring.h"

struct TodoItem {
    std::string title, rubricItem, location, problem, fix;
};

struct StudyItem {
    std::string topic, why, how;
};

struct GradeResult {
    std::string summary;
    std::string estimatedScore;
    std::vector<TodoItem> todo;
    std::vector<StudyItem> study;
    std::vector<FoundError> errors;
};

struct GradeInput {
    Collected rubric;
    Collected context;
    Collected submission;
    std::vector<RecurringEntry> known;
};

nlohmann::json buildRequest(const std::string& model, const GradeInput& in);

// Text of the user prompt, without attachments (for --dry-run).
std::string buildPromptText(const GradeInput& in);

// Extracts the structured result from a Responses API reply.
// Throws std::runtime_error if the reply is incomplete, refused, or malformed.
GradeResult parseResponse(const nlohmann::json& response);
