// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariant>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <nx/vms/client/desktop/ui/graphics/items/overlays/area_tooltip_item.h>

namespace nx::vms::client::desktop::testkit {

/**
 * Wrapper class for QGraphicsItem.
 */
class GraphicsItemWrapper
{
    Q_GADGET
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString toolTip MEMBER m_toolTip)
    Q_PROPERTY(bool enabled MEMBER m_enabled)
    Q_PROPERTY(bool visible MEMBER m_visible)
    Q_PROPERTY(QString text READ text)

public:
    GraphicsItemWrapper(QGraphicsItem* item = nullptr)
    {
        // Save data from the item here as we cannot track lifetime of QGraphicsItem.
        m_valid = item;
        if (!m_valid)
            return;

        if (auto o = dynamic_cast<QObject*>(item))
        {
            m_object = o;
            m_type = m_object->metaObject()->className();
        }

        m_toolTip = item->toolTip();
        m_enabled = item->isEnabled();
        m_visible = item->isVisible();

        auto go = item;
        const QGraphicsView* v = go->scene()->views().first();
        const QRectF sceneRect = go->mapRectToScene(go->boundingRect());
        const QRect viewRect = v->mapFromScene(sceneRect).boundingRect();
        const QPoint topLeft = v->viewport()->mapToGlobal(viewRect.topLeft());
        const QPoint bottomRight = v->viewport()->mapToGlobal(viewRect.bottomRight());
        m_bounds = QRect(topLeft, bottomRight);

        if (auto o = qobject_cast<QGraphicsTextItem*>(m_object))
        {
            m_text = o->toHtml();
        }
        else if (auto tooltip = dynamic_cast<AreaTooltipItem*>(item))
        {
            m_text = tooltip->text();
            m_type = "nx::vms::client::desktop::AreaTooltipItem";
        }
    }

    QString type() const { return m_type; }

    bool isValid() const { return m_valid; }

    QObject* object() const { return m_object; }

    QRect bounds() const { return m_bounds; }

    QString text() const { return m_text; }

private:
    bool m_valid = false;
    bool m_visible = false;
    bool m_enabled = false;
    QString m_toolTip;
    QRect m_bounds;
    QPointer<QObject> m_object;
    QString m_text;
    QString m_type = "QGraphicsItem";
};

} // namespace nx::vms::client::desktop::testkit
