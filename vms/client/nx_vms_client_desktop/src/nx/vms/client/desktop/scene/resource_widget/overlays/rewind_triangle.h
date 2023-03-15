// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

class RewindTriangle: public QGraphicsWidget
{
    Q_OBJECT

public:
    RewindTriangle(bool fastForward, QGraphicsWidget* parent = nullptr);

private:
    void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* widget) override;

    QPixmap m_pixmap;
};
