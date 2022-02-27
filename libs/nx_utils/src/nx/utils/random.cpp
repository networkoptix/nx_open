// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "random.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>

namespace nx {
namespace utils {
namespace random {

QByteArray generate(int count)
{
    return generate<QByteArray>(count);
}

std::string generateName(int length)
{
    return generateName(*QRandomGenerator::global(), length);
}

} // namespace random
} // namespace utils
} // namespace nx
