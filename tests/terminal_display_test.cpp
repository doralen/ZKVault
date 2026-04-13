#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <string_view>

#include <pty.h>
#include <unistd.h>

#include "terminal/display.hpp"

namespace {

class ScopedFd {
public:
    explicit ScopedFd(int fd = -1) noexcept : fd_(fd) {}

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    ScopedFd& operator=(ScopedFd&& other) noexcept {
        if (this != &other) {
            Reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }

        return *this;
    }

    ~ScopedFd() {
        Reset();
    }

    int Get() const noexcept {
        return fd_;
    }

    void Reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }

        fd_ = new_fd;
    }

private:
    int fd_;
};

class ScopedFdRedirect {
public:
    ScopedFdRedirect(int target_fd, int replacement_fd)
        : target_fd_(target_fd), saved_fd_(::dup(target_fd)) {
        if (saved_fd_.Get() < 0) {
            throw std::runtime_error("failed to duplicate file descriptor");
        }

        if (::dup2(replacement_fd, target_fd_) != target_fd_) {
            throw std::runtime_error("failed to redirect file descriptor");
        }
    }

    ScopedFdRedirect(const ScopedFdRedirect&) = delete;
    ScopedFdRedirect& operator=(const ScopedFdRedirect&) = delete;

    ~ScopedFdRedirect() {
        if (saved_fd_.Get() >= 0) {
            ::dup2(saved_fd_.Get(), target_fd_);
        }
    }

private:
    int target_fd_;
    ScopedFd saved_fd_;
};

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value) : name_(name) {
        const char* existing = std::getenv(name_);
        if (existing != nullptr) {
            had_original_ = true;
            original_value_ = existing;
        }

        if (value == nullptr) {
            ::unsetenv(name_);
        } else {
            ::setenv(name_, value, 1);
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

    ~ScopedEnvVar() {
        if (had_original_) {
            ::setenv(name_, original_value_.c_str(), 1);
            return;
        }

        ::unsetenv(name_);
    }

private:
    const char* name_;
    bool had_original_ = false;
    std::string original_value_;
};

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

std::string ReadAvailable(int fd) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0) {
        throw std::runtime_error("failed to read file descriptor flags");
    }

    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw std::runtime_error("failed to enable nonblocking reads");
    }

    std::string output;
    char buffer[64];
    while (true) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));
            continue;
        }

        if (count == 0) {
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        throw std::runtime_error("failed to read output");
    }

    return output;
}

void TestClearScreenSequence() {
    Require(BuildClearScreenSequence() == "\x1b[2J\x1b[H",
            "clear-screen sequence should match ANSI erase+home");
}

void TestAlternateScreenSequences() {
    Require(BuildEnterAlternateScreenSequence() == "\x1b[?1049h\x1b[?25l",
            "enter-alternate-screen sequence should match");
    Require(BuildExitAlternateScreenSequence() == "\x1b[?25h\x1b[?1049l",
            "exit-alternate-screen sequence should match");
}

void TestInteractiveTerminalEmission() {
    int master_fd = -1;
    int slave_fd = -1;
    if (::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) != 0) {
        throw std::runtime_error("failed to open pseudo-terminal");
    }

    ScopedFd master(master_fd);
    ScopedFd slave(slave_fd);
    ScopedFdRedirect redirect_stdout(STDOUT_FILENO, slave.Get());
    ScopedEnvVar term("TERM", "xterm-256color");

    ClearTerminalScreenIfInteractive();

    Require(ReadAvailable(master.Get()) == BuildClearScreenSequence(),
            "interactive terminal should receive clear-screen sequence");
}

void TestDumbTerminalSuppression() {
    int master_fd = -1;
    int slave_fd = -1;
    if (::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) != 0) {
        throw std::runtime_error("failed to open pseudo-terminal");
    }

    ScopedFd master(master_fd);
    ScopedFd slave(slave_fd);
    ScopedFdRedirect redirect_stdout(STDOUT_FILENO, slave.Get());
    ScopedEnvVar term("TERM", "dumb");

    ClearTerminalScreenIfInteractive();

    Require(ReadAvailable(master.Get()).empty(),
            "dumb terminals should not receive clear-screen sequence");
}

void TestNonInteractiveSuppression() {
    int pipe_fds[2] = {-1, -1};
    if (::pipe(pipe_fds) != 0) {
        throw std::runtime_error("failed to create pipe");
    }

    ScopedFd read_end(pipe_fds[0]);
    ScopedFd write_end(pipe_fds[1]);
    {
        ScopedFdRedirect redirect_stdout(STDOUT_FILENO, write_end.Get());
        ScopedEnvVar term("TERM", "xterm-256color");
        ClearTerminalScreenIfInteractive();
    }

    write_end.Reset();
    Require(ReadAvailable(read_end.Get()).empty(),
            "non-interactive stdout should not receive clear-screen sequence");
}

}  // namespace

int main() {
    try {
        TestClearScreenSequence();
        TestAlternateScreenSequences();
        TestInteractiveTerminalEmission();
        TestDumbTerminalSuppression();
        TestNonInteractiveSuppression();
        return 0;
    } catch (const std::exception& ex) {
        return (std::fprintf(stderr, "terminal display test failed: %s\n", ex.what()), 1);
    }
}
