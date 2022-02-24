// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "drag_and_drop_p.h"

#include <QtGui/QDropEvent>
#include <QtCore/QCoreApplication>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <utils/common/delayed.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/utils/qml_utils.h>

namespace nx::vms::client::desktop {

namespace {

bool recursivelyContains(QQuickItem* item, const QPointF& point)
{
    if (!NX_ASSERT(item) || !item->contains(point))
        return false;

    for (auto parent = item->parentItem(); parent != nullptr; parent = parent->parentItem())
    {
        if (parent->clip())
            return recursivelyContains(parent, parent->mapFromItem(item, point));
    }

    return true;
}

} // namespace

bool DragMimeDataWatcher::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_watched)
        return false;

    const auto clearMimeData =
        [this]() { setMimeData({}); };

    switch (event->type())
    {
        case QEvent::Drop:
            executeLater(clearMimeData, this);
            [[fallthrough]];
        case QEvent::DragEnter:
        case QEvent::DragMove:
            setMimeData(const_cast<QMimeData*>(static_cast<QDropEvent*>(event)->mimeData()));
            break;

        case QEvent::DragLeave:
            clearMimeData();
            break;

        default:
            break;
    }

    return false;
}

QObject* DragMimeDataWatcher::watched() const
{
    return m_watched;
}

void DragMimeDataWatcher::setWatched(QObject* value)
{
    if (value == m_watched)
        return;

    if (m_watched)
    {
        m_watched->disconnect(this);
        m_watched->removeEventFilter(this);
    }

    const auto emitWatchedChanged =
        [this]() { emit watchedChanged({}); };

    m_watched = value;
    if (m_watched)
    {
        m_watched->installEventFilter(this);
        connect(m_watched.data(), &QObject::destroyed, this, emitWatchedChanged);
    }

    emitWatchedChanged();
}

QMimeData* DragMimeDataWatcher::mimeData() const
{
    return m_mimeData;
}

void DragMimeDataWatcher::setMimeData(QMimeData* value)
{
    if (value == m_mimeData)
        return;

    if (m_mimeData)
        m_mimeData->disconnect(this);

    const auto emitMimeDataChanged =
        [this]() { emit mimeDataChanged({}); };

    m_mimeData = value;
    if (m_mimeData)
        connect(m_mimeData.data(), &QObject::destroyed, this, emitMimeDataChanged);

    emitMimeDataChanged();
}

DragHoverWatcher::DragHoverWatcher(QQuickItem* parent): QObject(parent)
{
    qApp->installEventFilter(this);
}

QQuickItem* DragHoverWatcher::target() const
{
    return m_target;
}

void DragHoverWatcher::setTarget(QQuickItem* value)
{
    if (m_target == value)
        return;

    if (m_target)
        m_target->disconnect(this);

    m_target = value;
    setActive(false);

    if (m_target)
    {
        connect(m_target.data(), &QQuickItem::windowChanged, this,
            [this]() { setActive(false); });
    }

    emit targetChanged();
}

bool DragHoverWatcher::active() const
{
    return m_active;
}

void DragHoverWatcher::setActive(bool value)
{
    if (m_active == value)
        return;

    m_active = value;
    emit activeChanged();

    if (m_active)
        emit started({});
    else
        emit stopped({});
}

bool DragHoverWatcher::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_target)
        return false;

    const auto window = qobject_cast<QWindow*>(watched);
    if (!window)
        return false;

    switch (event->type())
    {
        case QEvent::DragEnter:
        case QEvent::DragMove:
        {
            if (window == QmlUtils::renderWindow(m_target))
                handleDrag(window->mapToGlobal(static_cast<QDropEvent*>(event)->pos()));
            return false;
        }

        case QEvent::DragLeave:
        {
            if (window == QmlUtils::renderWindow(m_target))
                handleDragStop();
            return false;
        }

        case QEvent::Drop:
        {
            handleDragStop();
            return false;
        }

        default:
            return false;
    }
}

void DragHoverWatcher::handleDrag(const QPoint& globalPos)
{
    if (!NX_ASSERT(m_target))
        return;

    const auto itemPos = m_target->mapFromGlobal(globalPos);
    if (recursivelyContains(m_target, itemPos))
    {
        setActive(true);
        emit positionChanged(itemPos, {});
    }
    else
    {
        setActive(false);
    }
}

void DragHoverWatcher::handleDragStop()
{
    setActive(false);
}

} // namespace nx::vms::client::desktop
