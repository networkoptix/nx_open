// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dot_notation_string.h"

#include <utility>

namespace nx::utils {

static auto popFront(const QString& hierarchicalName)
{
    const int pos = hierarchicalName.indexOf('.');
    return std::make_pair(
        hierarchicalName.left(pos),
        (pos == -1) ? QString() : hierarchicalName.right(hierarchicalName.length() - pos - 1));
}

void DotNotationString::add(const QString& str)
{
    auto [item, subitem] = popFront(std::move(str));
    DotNotationString& nested = (*this)[item];
    if (!subitem.isEmpty())
        nested.add(std::move(subitem));
}

void DotNotationString::add(const DotNotationString& map)
{
    for (auto item = map.begin(); item != map.end(); ++item)
    {
        DotNotationString& nested = (*this)[item.key()];
        if (!item.value().isEmpty())
            nested.add(std::move(item.value()));
    }
}

DotNotationString::iterator DotNotationString::findWithWildcard(const QString& str)
{
    auto it = find(str);

    if (it == end())
        it = find("*");

    return it;
}

DotNotationString::const_iterator DotNotationString::findWithWildcard(const QString& str) const
{
    auto it = find(str);

    if (it == end())
        it = find("*");

    return it;
}

bool DotNotationString::accepts(const QString& str) const
{
    if (empty())
        return true;

    return findWithWildcard(str) != end();
}

} // namespace nx::utils
