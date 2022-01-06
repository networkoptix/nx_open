// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
**
****************************************************************************/

#include "scene_position_listener.h"
#include <QtQuick/private/qquickitem_p.h>

namespace {

const QQuickItemPrivate::ChangeTypes kAncestorChangeTypes =
    QQuickItemPrivate::Geometry | QQuickItemPrivate::Parent | QQuickItemPrivate::Children;

const QQuickItemPrivate::ChangeTypes kItemChangeTypes =
    QQuickItemPrivate::Geometry | QQuickItemPrivate::Parent | QQuickItemPrivate::Destroyed;

} // namespace

ScenePositionListener::ScenePositionListener(QObject* parent):
    QObject(parent)
{
}

ScenePositionListener::~ScenePositionListener()
{
    if (!m_item)
        return;

    QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, kItemChangeTypes);
    removeAncestorListeners(m_item->parentItem());
}

void ScenePositionListener::setItem(QQuickItem *item)
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

void ScenePositionListener::setEnabled(bool enabled)
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

void ScenePositionListener::itemGeometryChanged(QQuickItem* item, QQuickGeometryChange, const QRectF&)
{
    Q_UNUSED(item)

    if (!m_item)
        return;

    updateScenePos();
}

void ScenePositionListener::itemParentChanged(QQuickItem* item, QQuickItem* parent)
{
    Q_UNUSED(item)

    if (!m_item)
        return;

    addAncestorListeners(parent);
}

void ScenePositionListener::itemChildRemoved(QQuickItem* item, QQuickItem* child)
{
    if (child == m_item)
        removeAncestorListeners(item);
    else if (isAncestor(child))
        removeAncestorListeners(child);
}

void ScenePositionListener::itemDestroyed(QQuickItem* item)
{
    // Remove all injected listeners.
    m_item = nullptr;
    QQuickItemPrivate::get(item)->removeItemChangeListener(this, kItemChangeTypes);
    removeAncestorListeners(item->parentItem());
}

void ScenePositionListener::updateScenePos()
{
    const QPointF& scenePos = m_item->mapToScene(QPointF(0, 0));
    if (m_scenePos != scenePos)
    {
        m_scenePos = scenePos;
        emit scenePosChanged();
    }
}

void ScenePositionListener::removeAncestorListeners(QQuickItem* item)
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

void ScenePositionListener::addAncestorListeners(QQuickItem* item)
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

bool ScenePositionListener::isAncestor(QQuickItem* item) const
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
