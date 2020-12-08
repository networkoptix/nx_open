#pragma once

#include <optional>

namespace nx {
namespace utils {

template <typename T>
T* getIf(std::optional<T>* optional) noexcept
{
    if (optional && optional->has_value())
        return &optional->value();

    return nullptr;
}

template <typename T>
const T* getIf(const std::optional<T>* optional) noexcept
{
    if (optional && optional->has_value())
        return &optional->value();

    return nullptr;
}

} // namespace utils
} // namespace nx
