#include "scene_position_listener.h"
#include <QtQuick/private/qquickitem_p.h>

#include <nx/utils/log/assert.h>

namespace {

const QQuickItemPrivate::ChangeTypes kAncestorChangeTypes =
    QQuickItemPrivate::Geometry | QQuickItemPrivate::Parent | QQuickItemPrivate::Children;

const QQuickItemPrivate::ChangeTypes kItemChangeTypes =
    QQuickItemPrivate::Geometry | QQuickItemPrivate::Parent | QQuickItemPrivate::Destroyed;

} // namespace

QnScenePositionListener::QnScenePositionListener(QObject* parent):
    QObject(parent)
{
}

QnScenePositionListener::~QnScenePositionListener()
{
    if (!m_item)
        return;

    QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, kItemChangeTypes);
    removeAncestorListeners(m_item->parentItem());
}

void QnScenePositionListener::setItem(QQuickItem *item)
{
    if (m_item == item)
        return;

    if (m_item)
    {
        QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, kItemChangeTypes);
        removeAncestorListeners(m_item->parentItem());
    }

    m_item = item;

    if (!m_item)
        return;

    if (m_enabled)
    {
        QQuickItemPrivate::get(m_item)->addItemChangeListener(this, kItemChangeTypes);
        addAncestorListeners(m_item->parentItem());
    }

    updateScenePos();
}

void QnScenePositionListener::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit enabledChanged();

    if (!m_item)
        return;

    if (enabled)
    {
        QQuickItemPrivate::get(m_item)->addItemChangeListener(this, kItemChangeTypes);
        addAncestorListeners(m_item->parentItem());
    }
    else
    {
        QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, kItemChangeTypes);
        removeAncestorListeners(m_item->parentItem());
    }

    updateScenePos();
}

void QnScenePositionListener::itemGeometryChanged(QQuickItem* item, const QRectF&, const QRectF&)
{
    Q_UNUSED(item)

    if (!m_item)
        return;

    updateScenePos();
}

void QnScenePositionListener::itemParentChanged(QQuickItem* item, QQuickItem* parent)
{
    Q_UNUSED(item)

    if (!m_item)
        return;

    addAncestorListeners(parent);
}

void QnScenePositionListener::itemChildRemoved(QQuickItem* item, QQuickItem* child)
{
    if (child == m_item)
        removeAncestorListeners(item);
    else if (isAncestor(child))
        removeAncestorListeners(child);
}

void QnScenePositionListener::itemDestroyed(QQuickItem* item)
{
    NX_ASSERT(m_item == item);

    // Remove all injected listeners.
    m_item = nullptr;
    QQuickItemPrivate::get(item)->removeItemChangeListener(this, kItemChangeTypes);
    removeAncestorListeners(item->parentItem());
}

void QnScenePositionListener::updateScenePos()
{
    const QPointF& scenePos = m_item->mapToScene(QPointF(0, 0));
    if (m_scenePos != scenePos)
    {
        m_scenePos = scenePos;
        emit scenePosChanged();
    }
}

void QnScenePositionListener::removeAncestorListeners(QQuickItem* item)
{
    if (item == m_item)
        return;

    QQuickItem* p = item;
    while (p)
    {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, kAncestorChangeTypes);
        p = p->parentItem();
    }
}

void QnScenePositionListener::addAncestorListeners(QQuickItem* item)
{
    if (item == m_item)
        return;

    QQuickItem* p = item;
    while (p)
    {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, kAncestorChangeTypes);
        p = p->parentItem();
    }
}

bool QnScenePositionListener::isAncestor(QQuickItem* item) const
{
    if (!m_item)
        return false;

    QQuickItem* parent = m_item->parentItem();
    while (parent)
    {
        if (parent == item)
            return true;

        parent = parent->parentItem();
    }
    return false;
}
