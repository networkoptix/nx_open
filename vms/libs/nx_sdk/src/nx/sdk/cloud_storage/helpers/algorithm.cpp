// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "algorithm.h"

#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace nx::sdk::cloud_storage {

using namespace std::chrono;

namespace {

template<typename T>
auto orderPred(bool isAscending, const T& c1, const T& c2)
{
    if (isAscending)
        return c1 < c2;

    return c2 < c1;
}

typedef struct
{
    int64_t one;
    uint64_t two;
} simd128i;

typedef struct
{
    uint64_t one;
    uint64_t two;
} simd128;


} // namespace

std::vector<std::string> split(const std::string& original, const std::string& separator)
{
    std::vector<std::string> result;
    enum class State
    {
        body,
        separator,
    } state = State::body;

    if (separator.empty())
    {
        result.push_back(original);
        return result;
    }

    size_t pos = 0;
    size_t sepPos = 0;
    std::string next;
    bool done = false;
    while (!done)
    {
        switch (state)
        {
            case State::body:
                if (original[pos] == separator[sepPos])
                {
                    state = State::separator;
                    if (!next.empty())
                        result.push_back(next);

                    next.clear();
                    break;
                }

                next.push_back(original[pos]);
                if (pos == original.size() - 1)
                {
                    result.push_back(next);
                    done = true;
                }

                ++pos;
                break;
            case State::separator:
                ++pos;
                ++sepPos;
                if (pos == original.size())
                {
                    done = true;
                    break;
                }

                if (sepPos == separator.size())
                {
                    sepPos = 0;
                    state = State::body;
                }

                break;
        }
    }

    return result;
}

std::vector<uint8_t> fromBase64(const std::string& data)
{
    static constexpr int8_t b64[] =
        { 62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,
        10,11, 12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,
        34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51 };

    enum class State { one, two, three, four } state = State::one;
    int8_t c;
    size_t index = 0;
    const auto decodeCh =
        [](char toDecode)
        {
            int i = (int)toDecode - 43;
            if (i < 0 || i >= (int) sizeof(b64))
                throw std::logic_error("Not b64 " + std::to_string(i));

            int8_t decoded = b64[i];
            if (decoded < 0)
                throw std::logic_error("Invalid b64 " + std::to_string(i));

            return decoded;
        };

    std::vector<uint8_t> result;
    result.reserve(std::round(data.size() * 0.8f));
    int8_t decoded = 0;
    while (true)
    {
        if (index == data.size() || data[index] == '=')
            break;

        switch (state)
        {
            case State::one:
                c = decodeCh(data[index]) << 0x2;
                state = State::two;
                break;
            case State::two:
                decoded = decodeCh(data[index]);
                c |= decoded >> 0x4;
                result.push_back(c);
                c = (decoded & 0xf) << 4;
                state = State::three;
                break;
            case State::three:
                decoded = decodeCh(data[index]);
                c |= decoded >> 2;
                result.push_back(c);
                c = decoded << 0x6;
                state = State::four;
                break;
            case State::four:
                c |= decodeCh(data[index]);
                result.push_back(c);
                state = State::one;
                break;
        }

        ++index;
    }

    return result;
}

std::string toBase64(const std::vector<uint8_t>& data)
{
    return toBase64(data.data(), data.size());
}

std::string toBase64(const uint8_t* data, int size)
{
    static constexpr const char* const b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    enum class State { one, two, three } state = State::one;
    std::string result;
    result.reserve((size_t) (size * 1.5f));
    char c;
    int index = 0;
    while (true)
    {
        if (index == size)
        {
            switch (state)
            {
                case State::one:
                    break;
                case State::two:
                    result.push_back(b64[(size_t) c]);
                    result.push_back('=');
                    result.push_back('=');
                    break;
                case State::three:
                    result.push_back(b64[(size_t) c]);
                    result.push_back('=');
                    break;
            }

            break;
        }

        switch (state)
        {
            case State::one:
                result.push_back(b64[(size_t) ((data[index] & 0xfc) >> 2)]);
                c = ((data[index] & 0x03) << 4) & 0xfc;
                state = State::two;
                break;
            case State::two:
                c |= ((data[index] & 0xf0) >> 4);
                result.push_back(b64[(size_t) c]);
                c = ((data[index] & 0xf) << 2) & 0xfc;
                state = State::three;
                break;
            case State::three:
                c |= ((data[index] & 0xc0) >> 6);
                result.push_back(b64[(size_t) c]);
                result.push_back(b64[(size_t) (data[index] & 0x3f)]);
                state = State::one;
                break;
        }

        ++index;
    }

    return result;
}

nx::sdk::cloud_storage::IDeviceAgent* findDeviceAgentById(
    const std::string& id,
    const std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>>& devices)
{
    auto resultIt = std::find_if(
        devices.cbegin(), devices.cend(),
        [&id](const auto& devicePtr) { return id == deviceId(devicePtr.get()); });

    if (resultIt == devices.cend())
        return nullptr;

    return resultIt->get();
}

} // namespace nx::sdk::cloud_storage
