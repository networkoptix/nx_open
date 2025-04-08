// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context_qml_initializer.h"

namespace nx::vms::client::core {

namespace {
static const QString kQmlPropertyName("appContext");
} // namespace

ApplicationContextQmlInitializer::ApplicationContextQmlInitializer(QObject* owner):
    base_type(owner, kQmlPropertyName)
{
}

void ApplicationContextQmlInitializer::storeToQmlContext(ApplicationContext* context)
{
    base_type::storeToQmlContext(context, kQmlPropertyName);
}

} // namespace nx::vms::client::core
