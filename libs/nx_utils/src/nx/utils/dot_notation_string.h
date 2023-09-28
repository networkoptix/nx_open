// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

namespace nx::utils {

/**
 * Helps to parse and navigate strings that represent a full or a partial path to a property in a
 * structure/object in the "dot notation", e.g. `server.cpu.vendor` or `response.error.message`.
 * It supports `*` in path strings to help with wildcard matching.
 */
class NX_UTILS_API DotNotationString : public QMap<QString, DotNotationString>
{
public:
    DotNotationString() = default;

    void add(const QString& str);
    void add(const DotNotationString& map);

    iterator findWithWildcard(const QString& str);
    const_iterator findWithWildcard(const QString& str) const;

    bool accepts(const QString& str) const;
};

} // namespace nx::utils
