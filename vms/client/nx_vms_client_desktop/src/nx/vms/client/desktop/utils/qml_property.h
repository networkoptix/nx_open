// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/vms/client/core/utils/qml_property.h>

namespace nx::vms::client::desktop {

template<typename T>
class QmlProperty: public core::QmlProperty<T>
{
public:
    using core::QmlProperty<T>::QmlProperty;
    using core::QmlProperty<T>::operator=;

    QmlProperty(QQuickWidget* quickWidget, const QString& propertyName)
    {
        if (!quickWidget) //< To allow debug suppression of QQuickWidget creation.
            return;

        const auto connection = this->connection();

        const auto updateTarget =
            [quickWidget, propertyName, connection = QPointer(connection)]()
            {
                if (connection)
                    connection->setTarget((QObject*) quickWidget->rootObject(), propertyName);
            };

        QObject::connect(quickWidget, &QQuickWidget::statusChanged, connection, updateTarget);
        updateTarget();
    }
};

} // namespace nx::vms::client::desktop
