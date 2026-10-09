#include "proc.h"

#include <cstdlib>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

ProcResult runProcess(const std::vector<std::string>& argv) {
    ProcResult res;
    if (argv.empty()) return res;

    int fds[2];
    if (pipe(fds) != 0) return res;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return res;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }

    close(fds[1]);
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) res.output.append(buf, n);
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    res.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return res;
}

bool onPath(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::stringstream ss(path);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        std::string full = dir + "/" + name;
        if (access(full.c_str(), X_OK) == 0) return true;
    }
    return false;
}
