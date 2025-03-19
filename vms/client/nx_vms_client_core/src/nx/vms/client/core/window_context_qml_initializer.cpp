// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_qml_initializer.h"

namespace nx::vms::client::core {

namespace {
static const QString kQmlPropertyName("windowContext");
} // namespace

WindowContextQmlInitializer::WindowContextQmlInitializer(QObject* owner):
    base_type(owner, kQmlPropertyName)
{
}

void WindowContextQmlInitializer::storeToQmlContext(WindowContext* context)
{
    base_type::storeToQmlContext(context, kQmlPropertyName);
}

} // namespace nx::vms::client::core
