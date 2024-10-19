// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "back_gesture_workaround.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJniObject>
#include <QtCore/QPointer>
#include <QtQuick/private/qquickmousearea_p.h>

#include <QtQuick/QQuickItem>

namespace nx::vms::client::mobile {

struct BackGestureWorkaround::Private: public QObject
{
    using base_type = QObject;

    Private();

    static void handleBackGestureStarted(); //< Called when modern back gesture is detected.

    bool eventFilter(QObject* watched, QEvent* event) override;

    using ItemPointer = QPointer<QQuickItem>;
    ItemPointer workaroundParentItem;
    ItemPointer clickArea; //< Receiver for the workarund-related clicks. Prevents UI toggle.
    static QPointer<QQuickItem> currentArea; //< Current mouse area to be workarounded.
};

QPointer<QQuickItem> BackGestureWorkaround::Private::currentArea;

void BackGestureWorkaround::Private::handleBackGestureStarted()
{
    if (!currentArea)
        return;

    // Since back gestuure changes movement event flow we send deactivation signal to the
    // current area. This prevents mouse area from hangs.
    currentArea->ungrabMouse();
    currentArea->ungrabTouchPoints();
}

BackGestureWorkaround::Private::Private()
{
    static const bool workaroundIsRegistered =
        [this]()
        {
            JNINativeMethod methods[] {{"notifyBackGestureStarted", "()V",
                reinterpret_cast<void *>(&BackGestureWorkaround::Private::handleBackGestureStarted)}};

            QJniEnvironment env;
            const QJniObject javaClass(
                "com/nxvms/mobile/utils/Android10BackGestureWorkaround");
            const jclass objectClass = env->GetObjectClass(javaClass.object<jobject>());
            env->RegisterNatives(objectClass, methods, sizeof(methods) / sizeof(methods[0]));
            env->DeleteLocalRef(objectClass);

            qApp->installEventFilter(this);
            return true;
        }();
}

bool BackGestureWorkaround::Private::eventFilter(QObject* watched, QEvent* event)
{
    const auto area = qobject_cast<QQuickItem*>(watched);
    if (!area)
        return base_type::eventFilter(watched, event);

    const auto type = event->type();
    if (type == QEvent::TouchBegin)
        currentArea = area;
    else if (type == QEvent::TouchEnd || type == QEvent::TouchCancel)
        currentArea = nullptr;

    return base_type::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------

BackGestureWorkaround::BackGestureWorkaround(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

BackGestureWorkaround::~BackGestureWorkaround()
{
}

void BackGestureWorkaround::setWorkaroundParentItem(QQuickItem* item)
{
    if (d->workaroundParentItem == item)
        return;

    if (d->workaroundParentItem)
    {
        // Remove workaround area from previous target item.
        d->workaroundParentItem->setParentItem(nullptr);
        d->workaroundParentItem->setParent(nullptr);
        d->workaroundParentItem->deleteLater();
    }

    d->workaroundParentItem = item;

    if (d->workaroundParentItem)
    {
        d->clickArea = new QQuickMouseArea(d->workaroundParentItem);
        d->clickArea->setWidth(1);
        d->clickArea->setHeight(1);
    }

    emit workaroundParentItemChanged();
}

QQuickItem* BackGestureWorkaround::workaroundParentItem() const
{
    return d->workaroundParentItem;
}

} // namespace nx::vms::client::mobile
