/**********************************************************
* Apr 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "random.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <random>


namespace nx {
namespace utils {

namespace {
struct RandomGenerationContext
{
    std::mutex mutex;
    std::random_device randomDevice;
};

RandomGenerationContext randomGenerationContext;
}

bool generateRandomData(std::int8_t* data, std::size_t count)
{
    try
    {
        std::lock_guard<std::mutex> lk(randomGenerationContext.mutex);
        std::uniform_int_distribution<int> distribution(
            std::numeric_limits<std::int8_t>::min(),
            std::numeric_limits<std::int8_t>::max());
        std::generate(
            data, data + count,
            [&distribution]
            {
                return distribution(randomGenerationContext.randomDevice);
            });
        return true;
    }
    catch (const std::exception& /*e*/)
    {
        return false;
    }
}

QByteArray generateRandomData(std::size_t count, bool* ok)
{
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    const bool result = 
        generateRandomData(reinterpret_cast<std::int8_t*>(data.data()), count);
    if (ok)
        *ok = result;
    return data;
}

}   // namespace nx
}   // namespace utils
