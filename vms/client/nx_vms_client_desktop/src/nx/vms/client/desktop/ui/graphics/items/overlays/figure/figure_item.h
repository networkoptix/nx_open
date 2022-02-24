// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsItem>

#include "types.h"

namespace nx::vms::client::desktop {

/** Renders figure and helps to interact with it on the scene. */
class FigureItem: public QObject, public QGraphicsItem
{
    Q_OBJECT

public:
    FigureItem(
        const figure::FigurePtr& figure,
        const figure::RendererPtr& renderer,
        QGraphicsItem* parentItem = nullptr,
        QObject* parentObject = nullptr);

    void setFigure(const figure::FigurePtr& value);
    figure::FigurePtr figure() const;

    bool containsMouse() const;

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    virtual QRectF boundingRect() const override;
    virtual QPainterPath shape() const override;

signals:
    void containsMouseChanged();
    void clicked();

protected:
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void setContainsMouse(bool value);

private:
    figure::FigurePtr m_figure;
    const figure::RendererPtr m_renderer;
    bool m_containsMouse = false;
};

} // namespace nx::vms::client::desktop
