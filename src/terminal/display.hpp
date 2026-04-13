#pragma once

#include <string>

bool ShouldEmitTerminalControlSequences(int fd);

std::string BuildClearScreenSequence();

std::string BuildEnterAlternateScreenSequence();

std::string BuildExitAlternateScreenSequence();

void ClearTerminalScreenIfInteractive();
