// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "geometry.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

namespace {

static QObject* createInstance(QQmlEngine* /*engine*/, QJSEngine* /*jsEngine*/)
{
    return new Geometry();
}

} // namespace

void Geometry::registerQmlType()
{
    qmlRegisterSingletonType<Geometry>("nx.vms.client.core", 1, 0, "Geometry", &createInstance);
}

} // namespace nx::vms::client::core
