// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

#include <nx/vms/client/desktop/ui/scene/instruments/instrument_events.h>

namespace nx::vms::client::desktop {

/**
 * Signalizes about mouse events sent to windows.
 */
class MouseSpy: public QObject
{
    Q_OBJECT

    explicit MouseSpy(QObject* parent = nullptr);

public:
    static MouseSpy* instance();

signals:
    void mouseMove(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mousePress(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseRelease(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
};

class MouseSpyInterface: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    static MouseSpy* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::MouseSpyInterface, QML_HAS_ATTACHED_PROPERTIES)
