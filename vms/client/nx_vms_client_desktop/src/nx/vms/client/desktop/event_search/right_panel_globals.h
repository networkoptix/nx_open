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

enum class FetchDirection
{
    earlier,
    later
};
Q_ENUM_NS(FetchDirection)

enum class FetchResult
{
    complete, //< Successful. There's no more data to fetch.
    incomplete, //< Successful. There's more data to fetch.
    failed, //< Unsuccessful.
    cancelled //< Cancelled.
};
Q_ENUM_NS(FetchResult)

enum class PreviewState
{
    initial,
    busy,
    ready,
    missing
};
Q_ENUM_NS(PreviewState)

void registerQmlType();

} // namespace RightPanel
} // namespace nx::vms::client::desktop
