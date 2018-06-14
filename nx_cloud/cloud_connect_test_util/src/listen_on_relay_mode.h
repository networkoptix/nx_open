#pragma once

#include <ostream>

#include <nx/utils/argument_parser.h>

namespace nx {
namespace cctu {

void printListenOnRelayOptions(std::ostream* output);

int runInListenOnRelayMode(const nx::utils::ArgumentParser& args);

} // namespace cctu
} // namespace nx
