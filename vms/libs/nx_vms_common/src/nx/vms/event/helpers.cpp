// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>

namespace nx::vms::event {

QStringList splitOnPureKeywords(const QString& keywords)
{
    auto list = nx::utils::smartSplit(keywords, ' ', Qt::SkipEmptyParts);
    for (auto it = list.begin(); it != list.end();)
    {
        if (it->length() >= 2 && it->startsWith('"') && it->endsWith('"'))
        {
            *it = it->mid(1, it->length() - 2);
            if (it->isEmpty())
            {
                it = list.erase(it);
                continue;
            }
        }
        ++it;
    }
    return list;
}

bool checkForKeywords(const QString& value, const QStringList& keywords)
{
    return keywords.isEmpty()
        || (!value.isEmpty()
            && nx::utils::find_if(keywords, [&](const auto& k) { return value.contains(k); }));
}

} // namespace nx::vms::event
