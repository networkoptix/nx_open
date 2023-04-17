// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>
#include <vector>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/ptr.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

bool toBool(std::string str);

bool startsWith(const std::string& str, const std::string& prefix);

template<typename T>
T clamp(const T& value, const T& lowerBound, const T& upperBound)
{
    if (value < lowerBound)
        return lowerBound;

    if (value > upperBound)
        return upperBound;

    return value;
}

std::vector<char> loadFile(const std::string& path);

std::string imageFormatFromPath(const std::string& path);

bool isHttpOrHttpsUrl(const std::string& path);

std::string join(const std::vector<std::string>& strings,
    const std::string& delimiter,
    const std::string& itemPrefix = std::string(),
    const std::string& itemPostfix = std::string());

std::map<std::string, std::string> toStdMap(const nx::sdk::Ptr<const nx::sdk::IStringMap>& sdkMap);

template<typename T>
class SimpleOptional
{

public:
    SimpleOptional() = default;

    SimpleOptional(const T& value):
        m_value(value),
        m_isInitialized(true)
    {
    }

    template<typename U>
    SimpleOptional(const SimpleOptional<U>& other):
        m_value(other.value()),
        m_isInitialized(other.isInitialized())
    {
    }

    const T* operator->() const
    {
        if (!m_isInitialized)
            return nullptr;

        return &m_value;
    }

    T* operator->()
    {
        if (!m_isInitialized)
            return nullptr;

        return &m_value;
    }

    const T& operator*() const
    {
        return m_value;
    }

    T& operator*()
    {
        return m_value;
    }

    template<typename U>
    SimpleOptional& operator=(const SimpleOptional<U>& other)
    {
        m_value = other.value();
        m_isInitialized = other.isInitialized();

        return *this;
    }

    template<typename U>
    SimpleOptional& operator=(U&& value)
    {
        m_value = std::forward<U>(value);
        m_isInitialized = true;

        return *this;
    }

    explicit operator bool() const { return m_isInitialized; }

    const T& value() const
    {
        return m_value;
    }

    bool isInitialized() const { return m_isInitialized; }

    void reset() { m_isInitialized = false; }

private:
    T m_value{};
    bool m_isInitialized = false;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
