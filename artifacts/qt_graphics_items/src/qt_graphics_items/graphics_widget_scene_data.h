// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QEvent>
#include <QtCore/QPointF>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QPointer>

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/log/assert.h>

#include "graphics_widget.h"

class GraphicsWidgetSceneData: public QObject {
    Q_OBJECT;
public:
    /** Event type for scene-wide layout requests. */
    static const QEvent::Type HandlePendingLayoutRequests = static_cast<QEvent::Type>(QEvent::User + 0x19FA);

    GraphicsWidgetSceneData(QGraphicsScene *scene, QObject *parent = nullptr):
        QObject(parent),
        scene(scene)
    {
        NX_ASSERT(scene);
    }

    virtual ~GraphicsWidgetSceneData() {
        return;
    }

    virtual bool event(QEvent *event) override {
        if(event->type() == HandlePendingLayoutRequests) {
            if(scene)
                GraphicsWidget::handlePendingLayoutRequests(scene.data());
            return true;
        } else {
            return QObject::event(event);
        }
    }

    QPointer<QGraphicsScene> scene;
    QHash<QGraphicsItem *, QPointF> movingItemsInitialPositions;
    QSet<QGraphicsWidget *> pendingLayoutWidgets;
};
