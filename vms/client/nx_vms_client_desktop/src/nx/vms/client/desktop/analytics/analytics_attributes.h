#pragma once

namespace nx::vms::client::desktop {

inline bool isAnalyticsAttributeHidden(const QString& name)
{
    return name.startsWith("nx.sys.") || name.endsWith(".sys.hidden");
}

} // namespace nx::vms::client::desktop
