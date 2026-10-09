#pragma once
#include <filesystem>
#include <string>

// Sends `file` to the Tailscale device `target` with `tailscale file cp`.
// Returns an empty string on success, or the error output.
std::string taildrop(const std::filesystem::path& file, const std::string& target);
