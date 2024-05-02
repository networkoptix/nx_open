// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "global_temporaries.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

GlobalTemporaries* GlobalTemporaries::instance()
{
    static GlobalTemporaries instance;
    return &instance;
}

void GlobalTemporaries::registerQmlType()
{
    qmlRegisterSingletonInstance<GlobalTemporaries>("nx.vms.client.core", 1, 0,
        "GlobalTemporaries", instance());
}

} // namespace nx::vms::client::core
