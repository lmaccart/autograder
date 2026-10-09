#include "bundle.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>

#include "archive.h"
#include "collect.h"
#include "pipeline.h"

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

void moveTo(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) return;
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing |
                           fs::copy_options::copy_symlinks);  // moved as links, never followed
    fs::remove_all(from);
}

// Unpacks one received item into `dest`.
void unpack(const fs::path& item, const fs::path& dest) {
    fs::create_directories(dest);
    if (fs::is_symlink(item))
        throw std::runtime_error(item.filename().string() + " is a symbolic link, which isn't allowed");
    if (fs::is_directory(item))
        fs::copy(item, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing |
                                 fs::copy_options::skip_symlinks);
    else if (isZip(item))
        extractZip(item, dest);
    else
        fs::copy_file(item, dest / item.filename(), fs::copy_options::overwrite_existing);
}

// Turns a role's files into one path for runGrading: the file itself if there
// is just one, otherwise a directory holding them all (zips extracted).
fs::path stage(const fs::path& root, const std::vector<std::string>& rel, const fs::path& dir) {
    if (rel.size() == 1) return root / rel[0];
    for (const auto& r : rel) {
        fs::path src = root / r;
        fs::path dst = dir / r;
        fs::create_directories(dst.parent_path());
        if (isZip(src)) extractZip(src, dst.parent_path() / dst.stem());
        else fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    }
    return dir;
}

std::string joined(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + v[i];
    return out.empty() ? "(none)" : out;
}

}  // namespace

std::vector<std::string> bundleFiles(const fs::path& root) {
    std::vector<std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (e.is_symlink() || !e.is_regular_file()) continue;  // never read through links
        fs::path rel = fs::relative(e.path(), root);
        if (ignoredPath(rel)) continue;
        files.push_back(rel.generic_string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

namespace {

// Splits "HW_3-final" into {"hw", "3", "final"}: breaks at non-alphanumerics
// and between letters and digits.
std::vector<std::string> tokens(const std::string& name) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&] {
        if (!cur.empty()) out.push_back(cur);
        cur.clear();
    };
    for (char ch : lower(name)) {
        unsigned char c = ch;
        if (!std::isalnum(c)) {
            flush();
            continue;
        }
        if (!cur.empty() && std::isdigit(c) != std::isdigit((unsigned char)cur.back())) flush();
        cur += ch;
    }
    flush();
    return out;
}

bool homeworkName(const std::string& name) {
    for (const auto& t : tokens(name))
        if (t == "hw" || t == "hwk" || t == "homework") return true;
    return false;
}

}  // namespace

BundlePlan sortBundle(const std::vector<std::string>& files) {
    // A zip of a folder puts everything under one wrapper folder (often named
    // "hw3" itself), so that folder says nothing about any particular file.
    std::string wrapper;
    if (!files.empty()) {
        auto slash = files[0].find('/');
        if (slash != std::string::npos) {
            wrapper = files[0].substr(0, slash + 1);
            for (const auto& f : files)
                if (f.rfind(wrapper, 0) != 0) wrapper.clear();
        }
    }

    BundlePlan plan;
    for (const auto& f : files) {
        bool rubric = false, homework = false;
        for (const auto& part : fs::path(f.substr(wrapper.size()))) {
            std::string name = part.string();
            rubric |= lower(name).find("rubric") != std::string::npos;
            homework |= homeworkName(name);
        }
        if (rubric) plan.rubric.push_back(f);
        else if (homework) plan.homework.push_back(f);
        else plan.context.push_back(f);
    }

    if (plan.rubric.empty())
        throw std::runtime_error("no rubric found: name the rubric file with \"rubric\" in it (e.g. rubric.pdf)");
    if (plan.homework.empty())
        throw std::runtime_error("no homework found: name your work with hw, hwk or homework "
                                 "(e.g. hw3.py, HW3/, homework3.zip)");
    return plan;
}

int gradeBundle(const Config& cfg, const std::vector<fs::path>& items, bool consume) {
    if (items.empty()) throw std::runtime_error("empty bundle");
    std::string stem = items.size() == 1 ? items[0].stem().string() : "bundle";
    fs::path job = appDir() / "jobs" / (safeName(stem) + "-" + now("%Y%m%d-%H%M%S"));
    fs::path root = job / "bundle";
    fs::create_directories(root);

    std::vector<fs::path> sources = items;
    if (consume) {
        // Take the items out of the inbox first so a failure can't loop forever.
        fs::create_directories(job / "received");
        for (auto& s : sources) {
            fs::path dest = job / "received" / s.filename();
            moveTo(s, dest);
            s = dest;
        }
    }
    // A single zip or folder becomes the root itself; several items sit side by side.
    for (const auto& s : sources) {
        bool single = sources.size() == 1 || !(fs::is_directory(s) || isZip(s));
        unpack(s, single ? root : root / (fs::is_directory(s) ? s.filename() : s.stem()));
    }

    auto files = bundleFiles(root);
    if (files.empty()) throw std::runtime_error("the bundle has no files in it");
    BundlePlan plan = sortBundle(files);
    std::cerr << "  rubric:   " << joined(plan.rubric) << "\n"
              << "  homework: " << joined(plan.homework) << "\n"
              << "  context:  " << joined(plan.context) << "\n";

    Config c = cfg;
    c.rubric = stage(root, plan.rubric, job / "rubric");
    c.solution = stage(root, plan.homework, job / "homework");
    c.files = plan.context.empty() ? fs::path() : stage(root, plan.context, job / "context");
    if (!cfg.nameSet) c.name = items.size() == 1 ? stem : fs::path(plan.homework[0]).stem().string();
    return runGrading(c);
}
