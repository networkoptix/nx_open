// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mouse_spy.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QWindow>
#include <QtQml/QtQml>

#include <utils/common/event_processors.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

MouseSpy::MouseSpy(QObject* parent):
    QObject(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    const auto kMouseEvents =
        {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease};

    installEventHandler(QCoreApplication::instance(), kMouseEvents, this,
        [this](QObject* watched, QEvent* event)
        {
            if (!qobject_cast<QWindow*>(watched))
                return;

            nx::vms::client::desktop::ui::scene::MouseEvent mouse(static_cast<QMouseEvent*>(event));
            switch (event->type())
            {
                case QEvent::MouseMove:
                    emit mouseMove(&mouse);
                    break;

                case QEvent::MouseButtonPress:
                    emit mousePress(&mouse);
                    break;

                case QEvent::MouseButtonRelease:
                    emit mouseRelease(&mouse);
                    break;

                default:
                    NX_ASSERT(false, "Should never happen");
                    break;
            }
        });
}

MouseSpy* MouseSpy::instance()
{
    static MouseSpy instance;
    QQmlEngine::setObjectOwnership(&instance, QQmlEngine::CppOwnership);
    return &instance;
}

MouseSpy* MouseSpyInterface::qmlAttachedProperties(QObject* /*parent*/)
{
    return MouseSpy::instance();
}

void MouseSpyInterface::registerQmlType()
{
    qmlRegisterType<MouseSpyInterface>("nx.vms.client.desktop", 1, 0, "MouseSpy");
}

} // namespace nx::vms::client::desktop
