// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_globals.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/client_core_globals.h>

namespace nx::vms::client::core {
namespace EventSearch {

void registerQmlTypes()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.core", 1, 0,
        "EventSearch", "EventSearch is a namespace");

    qRegisterMetaType<core::EventSearch::FetchDirection>();
    qRegisterMetaType<core::EventSearch::FetchResult>();
    qRegisterMetaType<core::EventSearch::PreviewState>();
    qRegisterMetaType<core::EventSearch::SearchType>();
}

FetchDirection directionFromSortOrder(Qt::SortOrder order)
{
    return order == Qt::AscendingOrder
        ? EventSearch::FetchDirection::newer
        : EventSearch::FetchDirection::older;
}

Qt::SortOrder sortOrderFromDirection(FetchDirection direction)
{
    return direction == EventSearch::FetchDirection::newer
        ? Qt::AscendingOrder
        : Qt::DescendingOrder;
}

} // namespace EventSearch
} // namespace nx::vms::client::core
