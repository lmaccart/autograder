#include <iostream>

#include "config.h"
#include "bundle.h"
#include "openai.h"
#include "pipeline.h"
#include "watch.h"

int main(int argc, char** argv) {
    Config cfg = loadConfig(argc, argv);
    if (cfg.watch) return watchInbox(cfg);
    try {
        if (!cfg.bundle.empty()) return gradeBundle(cfg, {cfg.bundle}, false);
        return runGrading(cfg);
    } catch (const ApiError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
