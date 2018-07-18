#pragma once

#include <ostream>

#include <nx/utils/argument_parser.h>

namespace nx {
namespace cctu {

void printTestRelayOptions(std::ostream* output);

int testRelay(const nx::utils::ArgumentParser& args);

} // namespace cctu
} // namespace nx
