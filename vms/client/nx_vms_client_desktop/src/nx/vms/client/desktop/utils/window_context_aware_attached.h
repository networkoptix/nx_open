// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtQmlIntegration/QtQmlIntegration>

namespace nx::vms::client::desktop {

class WindowContext;

class WindowContextAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context MEMBER m_windowContext CONSTANT)

public:
    WindowContextAttached(WindowContext* context);

signals:
    void beforeSystemChanged();
    void systemChanged();
    void userIdChanged();

private:
    WindowContext* m_windowContext = nullptr;
};

/**
 * Allows to use WindowContext inside QML as attached properties.
 */
class WindowContextAwareHelper: public QObject
{
    Q_OBJECT
    QML_ATTACHED(WindowContextAttached)

public:
    static WindowContextAttached* qmlAttachedProperties(QObject* object);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

Q_DECLARE_OPAQUE_POINTER(nx::vms::client::desktop::WindowContext*);
