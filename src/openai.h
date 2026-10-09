#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

struct ApiError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// POSTs `body` to {baseUrl}/responses and returns the parsed JSON.
// Retries rate limits (429), server errors and network failures, honouring
// retry-after / x-ratelimit-reset-* headers, for up to `maxWaitSeconds` total.
// Throws ApiError on permanent failure.
nlohmann::json postResponses(const std::string& baseUrl, const std::string& apiKey,
                             const nlohmann::json& body, int maxWaitSeconds);

// Parses durations like "1s", "6m0s", "250ms", "1.5s", "2h3m" or a bare
// number of seconds. Returns seconds, or -1 if unparseable.
double parseDuration(const std::string& s);
