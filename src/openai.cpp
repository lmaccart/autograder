#include "openai.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <thread>

using nlohmann::json;

namespace {

struct HttpResponse {
    long status = 0;
    std::string body;
    std::map<std::string, std::string> headers;  // lowercase keys
    std::string curlError;
};

size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string line(ptr, size * nmemb);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        std::string val = line.substr(colon + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        (*static_cast<std::map<std::string, std::string>*>(userdata))[key] = val;
    }
    return size * nmemb;
}

HttpResponse httpPost(const std::string& url, const std::string& apiKey, const std::string& payload) {
    HttpResponse res;
    CURL* curl = curl_easy_init();
    if (!curl) {
        res.curlError = "curl_easy_init failed";
        return res;
    }
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, ("Authorization: Bearer " + apiKey).c_str());
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res.headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 900L);  // reasoning models can take a while
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) res.curlError = curl_easy_strerror(rc);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.status);

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return res;
}

std::string errorMessage(const HttpResponse& r) {
    try {
        auto j = json::parse(r.body);
        if (j.contains("error") && j["error"].contains("message"))
            return j["error"]["message"].get<std::string>();
    } catch (...) {
    }
    return r.body.substr(0, 500);
}

// Seconds the server asked us to wait, or -1 if it didn't say.
double serverWait(const HttpResponse& r) {
    double best = -1;
    for (const char* h : {"retry-after", "x-ratelimit-reset-requests", "x-ratelimit-reset-tokens"}) {
        auto it = r.headers.find(h);
        if (it == r.headers.end()) continue;
        double s = parseDuration(it->second);
        if (s > best) best = s;
    }
    return best;
}

}  // namespace

double parseDuration(const std::string& s) {
    if (s.empty()) return -1;
    double total = 0;
    size_t i = 0;
    bool any = false;
    while (i < s.size()) {
        size_t start = i;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) ++i;
        if (start == i) return -1;
        double v = std::stod(s.substr(start, i - start));
        std::string unit;
        while (i < s.size() && std::isalpha((unsigned char)s[i])) unit += s[i++];
        if (unit.empty() || unit == "s") total += v;
        else if (unit == "ms") total += v / 1000;
        else if (unit == "m") total += v * 60;
        else if (unit == "h") total += v * 3600;
        else return -1;
        any = true;
    }
    return any ? total : -1;
}

json postResponses(const std::string& baseUrl, const std::string& apiKey, const json& body,
                   int maxWaitSeconds) {
    static bool curlReady = (curl_global_init(CURL_GLOBAL_DEFAULT) == 0);
    if (!curlReady) throw ApiError("curl_global_init failed");

    const std::string url = baseUrl + "/responses";
    const std::string payload = body.dump();
    std::mt19937 rng(std::random_device{}());
    double waited = 0;
    double backoff = 5;

    for (int attempt = 1;; ++attempt) {
        HttpResponse r = httpPost(url, apiKey, payload);

        if (r.curlError.empty() && r.status >= 200 && r.status < 300) {
            try {
                return json::parse(r.body);
            } catch (const json::exception& e) {
                throw ApiError(std::string("could not parse API response: ") + e.what());
            }
        }

        std::string why;
        if (!r.curlError.empty()) {
            why = "network error: " + r.curlError;
        } else if (r.status == 429) {
            std::string msg = errorMessage(r);
            // A request bigger than the per-minute token limit can never succeed.
            if (msg.find("Request too large") != std::string::npos ||
                msg.find("insufficient_quota") != std::string::npos ||
                msg.find("exceeded your current quota") != std::string::npos)
                throw ApiError("OpenAI rejected the request (429): " + msg +
                               "\nTry lowering --max-chars or trimming the input zips.");
            why = "rate limited: " + msg;
        } else if (r.status >= 500) {
            why = "server error " + std::to_string(r.status);
        } else {
            throw ApiError("OpenAI API error " + std::to_string(r.status) + ": " + errorMessage(r));
        }

        double wait = serverWait(r);
        if (wait <= 0) {
            std::uniform_real_distribution<double> jitter(0, backoff * 0.25);
            wait = backoff + jitter(rng);
            backoff = std::min(backoff * 2, 120.0);
        } else {
            wait += 1;  // small cushion past the reset time
        }

        if (waited + wait > maxWaitSeconds)
            throw ApiError("giving up after " + std::to_string((int)waited) + "s of retries (" + why + ")");

        std::cerr << "[attempt " << attempt << "] " << why << "\n  waiting " << (int)(wait + 0.5)
                  << "s before retrying...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds((long)(wait * 1000)));
        waited += wait;
    }
}
