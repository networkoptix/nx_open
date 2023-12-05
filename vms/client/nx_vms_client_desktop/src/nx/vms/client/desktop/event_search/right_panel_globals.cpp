// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "right_panel_globals.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace RightPanel {

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "RightPanel", "RightPanel is a namespace");

    qmlRegisterUncreatableType<EventCategory>("nx.vms.client.desktop", 1, 0,
        "EventCategory", "Cannot create EventCategory");

    qRegisterMetaType<Tab>();
    qRegisterMetaType<EventCategory>();
    qRegisterMetaType<nx::vms::api::EventType>();
}

} // namespace RightPanel
} // namespace nx::vms::client::desktop
