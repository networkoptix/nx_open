// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/log/log_main.h>

namespace nx::log {

NX_REFLECTION_ENUM_CLASS(LogName,
    main,
    http,
    system
);

extern const NX_UTILS_API nx::log::Tag kMainTag;
extern const NX_UTILS_API nx::log::Tag kHttpTag;
extern const NX_UTILS_API nx::log::Tag kSystemTag;

constexpr size_t kLogNamesCount =
    nx::reflect::enumeration::visitAllItems<LogName>(
        [](auto&&... items)
        {
            return sizeof...(items);
        });

NX_UTILS_API std::array<QString, kLogNamesCount> getCompatLoggerNames();
NX_UTILS_API std::shared_ptr<nx::log::AbstractLogger> getLogger(const LogName id);

} // namespace nx::log
