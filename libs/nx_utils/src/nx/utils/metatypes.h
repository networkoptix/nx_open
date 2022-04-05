// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QMetaType>

namespace nx::utils {

struct NX_UTILS_API Metatypes
{
    static void initialize();
};

} // namespace nx::utils

Q_DECLARE_METATYPE(std::chrono::hours)
Q_DECLARE_METATYPE(std::chrono::minutes)
Q_DECLARE_METATYPE(std::chrono::seconds)
Q_DECLARE_METATYPE(std::chrono::milliseconds)
Q_DECLARE_METATYPE(std::chrono::microseconds)
