// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_globals.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop::workbench::timeline {

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "Timeline", "Timeline is a namespace");
}

} // namespace nx::vms::client::desktop::workbench::timeline
