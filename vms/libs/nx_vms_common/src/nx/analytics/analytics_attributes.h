// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::analytics {

inline bool isAnalyticsAttributeHidden(const QString& name)
{
    static const QString kSystem = "nx.sys.";
    static const QString kHidden = ".sys.hidden";
    return name.startsWith(kSystem) || name.endsWith(kHidden);
}

} // namespace nx::analytics
