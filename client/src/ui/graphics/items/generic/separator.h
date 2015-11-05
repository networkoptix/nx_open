
#pragma once

#include <ui/graphics/items/generic/framed_widget.h>

class QnSeparator : public QnFramedWidget
{
    Q_OBJECT

    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)

public:
    QnSeparator(QGraphicsItem *parent = nullptr);

    QnSeparator(const QColor &color
        , QGraphicsItem *parent = nullptr);

    virtual ~QnSeparator();

public:
    qreal lineWidth() const;

    void setLineWidth(qreal lineWidth
        , bool changeHeight = false);

    QColor lineColor() const;

    void setLineColor(const QColor &color);

private:
    void init();

    void updateLinePos();

    QSizeF m_lastSize;
};