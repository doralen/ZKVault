#include "terminal/display.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unistd.h>

bool ShouldEmitTerminalControlSequences(int fd) {
    if (::isatty(fd) == 0) {
        return false;
    }

    const char* term = std::getenv("TERM");
    if (term == nullptr || std::string_view(term).empty() ||
        std::string_view(term) == "dumb") {
        return false;
    }

    return true;
}

std::string BuildClearScreenSequence() {
    return "\x1b[2J\x1b[H";
}

std::string BuildEnterAlternateScreenSequence() {
    return "\x1b[?1049h\x1b[?25l";
}

std::string BuildExitAlternateScreenSequence() {
    return "\x1b[?25h\x1b[?1049l";
}

void ClearTerminalScreenIfInteractive() {
    if (!ShouldEmitTerminalControlSequences(STDOUT_FILENO)) {
        return;
    }

    std::cout << BuildClearScreenSequence();
    std::cout.flush();
}
