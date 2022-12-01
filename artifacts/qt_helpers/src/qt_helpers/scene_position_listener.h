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

#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qset.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

Q_MOC_INCLUDE("QtQuick/QQuickItem")

class QQuickItem;

class ScenePositionListener:
    public QObject,
    public QQuickItemChangeListener
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem)
    Q_PROPERTY(QPointF scenePos READ scenePos NOTIFY scenePosChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit ScenePositionListener(QObject* parent = nullptr);
    ~ScenePositionListener();

    QQuickItem* item() const { return m_item; }
    void setItem(QQuickItem* item);

    QPointF scenePos() const { return m_scenePos; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void scenePosChanged();
    void enabledChanged();

protected:
    void itemGeometryChanged(QQuickItem* item, const QRectF&, const QRectF&);
    void itemParentChanged(QQuickItem* item, QQuickItem* parent);
    void itemChildRemoved(QQuickItem* item, QQuickItem* child);
    void itemDestroyed(QQuickItem* item);

private:
    void updateScenePos();

    void removeAncestorListeners(QQuickItem* item);
    void addAncestorListeners(QQuickItem* item);

    bool isAncestor(QQuickItem* item) const;

    bool m_enabled = true;
    QPointF m_scenePos;
    QQuickItem* m_item = nullptr;
};
