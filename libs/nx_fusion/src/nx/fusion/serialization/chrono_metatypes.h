#pragma once

#include <chrono>
#include <QtCore/QMetaType>

Q_DECLARE_METATYPE(std::chrono::milliseconds)

static_assert(sizeof(std::chrono::milliseconds::rep) >= sizeof(qint64),
    "std::chrono::milliseconds should hold at least qint64 because we use value qint64::max().");
