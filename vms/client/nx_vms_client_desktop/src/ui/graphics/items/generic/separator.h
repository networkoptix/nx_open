// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QVector>
#include <QtWidgets/QGraphicsWidget>

struct SeparatorAreaProperties
{
    qreal width;
    QColor color;

    SeparatorAreaProperties();

    SeparatorAreaProperties(qreal width
        , const QColor &color);
};
typedef QVector<SeparatorAreaProperties> SeparatorAreas;

class QnSeparator : public QGraphicsWidget
{
    Q_OBJECT

public:
    QnSeparator(
        Qt::Orientation orientation,
        qreal width,
        const QColor& color,
        QGraphicsItem* parent = nullptr);

    QnSeparator(
        Qt::Orientation orientation,
        const SeparatorAreas& areas,
        QGraphicsItem* parent = nullptr);

    virtual ~QnSeparator();

private:
    void init();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    typedef QPair<QColor, QRectF> SeparatorRectProps;
    typedef QVector<SeparatorRectProps> SeparatorRectPropsVector;

    void updatePixmap();

private:
    Qt::Orientation m_orientation;
    const SeparatorAreas m_areas;
    QPixmap m_pixmap;
};
