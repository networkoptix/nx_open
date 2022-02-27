// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_opacity_mixin.h"

#include <QtCore/QEvent>
#include <QtCore/QDynamicPropertyChangeEvent>

namespace {

constexpr auto kOpacityPropertyName = "opacity";

} // namespace

const char* opacityPropertyName()
{
    return kOpacityPropertyName;
}

bool WidgetOpacityMixinEventFilter::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::DynamicPropertyChange)
    {
        QDynamicPropertyChangeEvent* propertyChangedEvent = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (propertyChangedEvent->propertyName() == opacityPropertyName())
            emit opacityChanged();
        return true;
    }

    return QObject::eventFilter(obj, event);
}
