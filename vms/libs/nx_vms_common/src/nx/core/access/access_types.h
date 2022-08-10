// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

namespace nx {
namespace core {
namespace access {

/** Types of base providers, sorted by priority. */
enum class Source
{
    none,
    permissions,    //< Accessible by permissions.
    shared,         //< Accessible by direct sharing.
    layout,         //< Accessible by placing on shared layout.
    videowall,      //< Accessible by placing on videowall.
    intercom //< Accessible as an intercom child.
};

inline uint qHash(Source value, uint seed)
{
    return ::qHash(static_cast<int>(value), seed);
}

enum class Mode
{
    cached,
    direct
};

} // namespace access
} // namespace core
} // namespace nx
