#include "taildrop.h"

#include "proc.h"

std::string taildrop(const std::filesystem::path& file, const std::string& target) {
    auto r = runProcess({"tailscale", "file", "cp", file.string(), target + ":"});
    if (r.exitCode == 0) return "";
    return r.output.empty() ? "tailscale exited with code " + std::to_string(r.exitCode) : r.output;
}
