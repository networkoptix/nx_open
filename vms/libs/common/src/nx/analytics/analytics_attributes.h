#pragma once

namespace nx::analytics {

inline bool isAnalyticsAttributeHidden(const QString& name)
{
    return name.startsWith("nx.sys.") || name.endsWith(".sys.hidden");
}

} // namespace nx::analytics
