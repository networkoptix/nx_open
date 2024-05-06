// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "right_panel_globals.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace RightPanel {

VmsEventGroup::VmsEventGroup(
    Type type,
    nx::vms::api::EventType id,
    const QString& name,
    const QString& any)
    :
    VmsEvent{.id = id, .name = name},
    type(type),
    any(any)
{
}

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "RightPanel", "RightPanel is a namespace");

    qmlRegisterUncreatableType<VmsEvent>("nx.vms.client.desktop", 1, 0,
        "VmsEvent", "Cannot create VmsEvent");

    qmlRegisterUncreatableType<VmsEventGroup>("nx.vms.client.desktop", 1, 0,
        "VmsEventGroup", "Cannot create VmsEventGroup");

    qRegisterMetaType<FetchDirection>();
    qRegisterMetaType<FetchResult>();
    qRegisterMetaType<PreviewState>();
    qRegisterMetaType<VmsEvent>();
    qRegisterMetaType<VmsEventGroup>();
    qRegisterMetaType<nx::vms::api::EventType>();
}

} // namespace RightPanel
} // namespace nx::vms::client::desktop
