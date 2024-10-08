// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {
namespace EventSearch {

Q_NAMESPACE_EXPORT(NX_VMS_CLIENT_CORE_API)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

NX_REFLECTION_ENUM_CLASS(FetchDirection,
    older, //< Descending data order.
    newer //< Ascending data order.
);
Q_ENUM_NS(FetchDirection)

NX_REFLECTION_ENUM_CLASS(FetchResult,
    complete, //< Successful. There's no more data to fetch.
    incomplete, //< Successful. There's more data to fetch.
    failed, //< Unsuccessful.
    cancelled //< Cancelled.
);
Q_ENUM_NS(FetchResult)

NX_REFLECTION_ENUM_CLASS(CameraSelection,
    all,
    layout,
    current,
    custom
);
Q_ENUM_NS(CameraSelection)

NX_REFLECTION_ENUM_CLASS(TimeSelection,
    anytime,
    day,
    week,
    month,
    selection
);
Q_ENUM_NS(TimeSelection)

NX_REFLECTION_ENUM_CLASS(PreviewState,
    initial,
    busy,
    ready,
    missing
);
Q_ENUM_NS(PreviewState)

NX_REFLECTION_ENUM_CLASS(SearchType,
    invalid = -1,
    notifications,
    motion,
    bookmarks,
    events,
    analytics
);
Q_ENUM_NS(SearchType)


void registerQmlTypes();

NX_VMS_CLIENT_CORE_API FetchDirection directionFromSortOrder(Qt::SortOrder order);
NX_VMS_CLIENT_CORE_API Qt::SortOrder sortOrderFromDirection(FetchDirection direction);

} // namespace EventSearch
} // namespace nx::vms::client::core
