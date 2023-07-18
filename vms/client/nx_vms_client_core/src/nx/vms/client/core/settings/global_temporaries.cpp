// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "global_temporaries.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

GlobalTemporaries* GlobalTemporaries::instance()
{
    static GlobalTemporaries theInstance;
    return &theInstance;
}

void GlobalTemporaries::registerQmlType()
{
    const auto theInstance = GlobalTemporaries::instance();
    QQmlEngine::setObjectOwnership(theInstance, QQmlEngine::CppOwnership);

    qmlRegisterSingletonInstance<GlobalTemporaries>("nx.vms.client.core", 1, 0,
        "GlobalTemporaries", theInstance);
}

} // namespace nx::vms::client::core
