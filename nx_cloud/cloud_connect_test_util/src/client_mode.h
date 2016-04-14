#pragma once

#include "listen_mode.h"

namespace nx {
namespace cctu {

void printConnectOptions(std::ostream* const outStream);
int runInConnectMode(const std::multimap<QString, QString>& args);

void printHttpClientOptions(std::ostream* const outStream);
int runInHttpClientMode(const std::multimap<QString, QString>& args);

} // namespace cctu
} // namespace nx
