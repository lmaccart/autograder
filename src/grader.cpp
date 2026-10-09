#include "grader.h"

#include <stdexcept>

#include "openai.h"

using nlohmann::json;

namespace {

const char* kInstructions = R"(You are a meticulous, fair teaching assistant grading a student's submission.
You are given the grading rubric, the student's submission, and sometimes context files for
the assignment (spec, starter code, data, etc.). The student will read your output to improve this
submission and to get better in future assignments, so be specific and actionable.

Grade strictly against the rubric. For every rubric item where points would be lost, or where
the work is at risk, produce a TODO item:
- title: short imperative summary of the change
- rubric_item: the rubric item it relates to (quote or name it)
- location: file and line/function, or document section, where the change is needed
- problem: what is wrong and why it costs points, in depth
- fix: concrete, step-by-step instructions for fixing it (include code or wording where useful)
Order TODO items by points at stake, highest first.

study: concepts or skills the student should study to avoid these mistakes in future
assignments. Each topic must be tied to what actually went wrong ("why") and give a concrete
way to practice or learn it ("how").

errors: the general, reusable categories of mistake made in this submission (e.g. "Off-by-one
loop bounds", "Missing input validation", "Unlabeled plot axes"). Keep names short, generic and
canonical so they can be tracked across assignments. You are given the student's list of
previously recorded recurring errors: if a mistake is the same kind as one already on the
list, set existing_match to that entry's exact name; otherwise set existing_match to "".

summary: an overall assessment (strengths, weaknesses, biggest risks) in a few paragraphs.
estimated_score: your estimate of the score per the rubric, e.g. "84/100", with a short
breakdown if the rubric has sections.

If the submission is excellent and nothing needs changing, return empty todo/errors lists.)";

json stringProp() { return json{{"type", "string"}}; }

json objectSchema(const std::vector<std::string>& fields) {
    json props = json::object();
    for (const auto& f : fields) props[f] = stringProp();
    return json{{"type", "object"}, {"properties", props}, {"required", fields}, {"additionalProperties", false}};
}

json resultSchema() {
    json props = {
        {"summary", stringProp()},
        {"estimated_score", stringProp()},
        {"todo", {{"type", "array"}, {"items", objectSchema({"title", "rubric_item", "location", "problem", "fix"})}}},
        {"study", {{"type", "array"}, {"items", objectSchema({"topic", "why", "how"})}}},
        {"errors", {{"type", "array"}, {"items", objectSchema({"name", "category", "description", "existing_match"})}}},
    };
    return json{{"type", "object"},
                {"properties", props},
                {"required", {"summary", "estimated_score", "todo", "study", "errors"}},
                {"additionalProperties", false}};
}

void appendSection(std::string& out, const std::string& title, const Collected& c) {
    out += "########## " + title + " ##########\n\n";
    if (c.text.empty() && c.pdfs.empty()) out += "(no readable files)\n\n";
    out += c.text;
    for (const auto& p : c.pdfs) out += "[attached PDF: " + p.name + "]\n";
    if (!c.pdfs.empty()) out += "\n";
}

std::string getStr(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}

}  // namespace

std::string buildPromptText(const GradeInput& in) {
    std::string text;
    text += "########## PREVIOUSLY RECORDED RECURRING ERRORS ##########\n\n";
    if (in.known.empty()) {
        text += "(none yet)\n\n";
    } else {
        for (const auto& e : in.known)
            text += "- " + e.name + " [" + e.category + "]: " + e.description + "\n";
        text += "\n";
    }
    appendSection(text, "RUBRIC", in.rubric);
    if (!in.context.text.empty() || !in.context.pdfs.empty())
        appendSection(text, "ASSIGNMENT CONTEXT FILES", in.context);
    appendSection(text, "STUDENT SUBMISSION (grade this)", in.submission);
    return text;
}

json buildRequest(const std::string& model, const GradeInput& in) {
    json content = json::array();
    content.push_back({{"type", "input_text"}, {"text", buildPromptText(in)}});
    for (const Collected* c : {&in.rubric, &in.context, &in.submission})
        for (const auto& p : c->pdfs)
            content.push_back({{"type", "input_file"},
                               {"filename", p.name},
                               {"file_data", "data:application/pdf;base64," + p.base64}});

    return json{
        {"model", model},
        {"instructions", kInstructions},
        {"input", json::array({{{"role", "user"}, {"content", content}}})},
        {"text", {{"format", {{"type", "json_schema"}, {"name", "grading_result"}, {"strict", true}, {"schema", resultSchema()}}}}},
    };
}

GradeResult parseResponse(const json& response) {
    json j = structuredOutput(response);

    GradeResult r;
    r.summary = getStr(j, "summary");
    r.estimatedScore = getStr(j, "estimated_score");
    for (const auto& t : j.value("todo", json::array()))
        r.todo.push_back({getStr(t, "title"), getStr(t, "rubric_item"), getStr(t, "location"),
                          getStr(t, "problem"), getStr(t, "fix")});
    for (const auto& s : j.value("study", json::array()))
        r.study.push_back({getStr(s, "topic"), getStr(s, "why"), getStr(s, "how")});
    for (const auto& e : j.value("errors", json::array()))
        r.errors.push_back({getStr(e, "name"), getStr(e, "category"), getStr(e, "description"),
                            getStr(e, "existing_match")});
    return r;
}
