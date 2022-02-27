// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <QtWidgets/QGraphicsWidget>

#include <qt_graphics_items/graphics_widget.h>

class QGraphicsView;
class Instrument;

/**
 * A graphics widget that has the same coordinate system as a viewport.
 *
 * Useful for placing UI controls.
 */
class QnGuiElementsWidget: public GraphicsWidget
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    QnGuiElementsWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {});
    virtual ~QnGuiElementsWidget();

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private slots:
    void updateTransform(QGraphicsView *view);
    void updateSize(QGraphicsView *view);

private:
    QPointer<Instrument> m_instrument;
};
