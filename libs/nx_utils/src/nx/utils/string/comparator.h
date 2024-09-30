// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHashFunctions>
#include <QtCore/QString>

namespace nx::utils {

struct CaseInsensitiveStringHasher
{
    bool operator()(const QString& key) const
    {
        return ::qHash(key.toLower());
    }
};

struct CaseInsensitiveStringEqual
{
    bool operator()(const QString& lhs, const QString& rhs) const
    {
        return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
    }
};

struct CaseInsensitiveStringCompare
{
    bool operator()(const QString& lhs, const QString& rhs) const
    {
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) < 0;
    }
};

} // namespace nx::utils
