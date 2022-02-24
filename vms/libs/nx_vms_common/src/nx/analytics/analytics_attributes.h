// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::analytics {

inline bool isAnalyticsAttributeHidden(const QString& name)
{
    return name.startsWith("nx.sys.") || name.endsWith(".sys.hidden");
}

} // namespace nx::analytics
