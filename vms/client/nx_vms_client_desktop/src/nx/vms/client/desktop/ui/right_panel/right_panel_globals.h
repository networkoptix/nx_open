#pragma once

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {
namespace RightPanel {

Q_NAMESPACE

enum class Tab
{
    invalid = -1,
    notifications,
    motion,
    bookmarks,
    events,
    analytics
};
Q_ENUM_NS(Tab)

void registerQmlType();

} // namespace RightPanel
} // namespace nx::vms::client::desktop
