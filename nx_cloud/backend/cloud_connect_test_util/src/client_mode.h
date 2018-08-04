#pragma once

#include "listen_mode.h"
#include <nx/utils/argument_parser.h>

namespace nx {
namespace cctu {

void printConnectOptions(std::ostream* const outStream);
int runInConnectMode(const nx::utils::ArgumentParser& args);

void printHttpClientOptions(std::ostream* const outStream);
int runInHttpClientMode(const nx::utils::ArgumentParser& args);

} // namespace cctu
} // namespace nx
