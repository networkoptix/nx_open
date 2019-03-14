#pragma once

#include <ui/graphics/items/generic/framed_widget.h>

class QnControlBackgroundWidget: public QnFramedWidget {
    Q_OBJECT
    Q_PROPERTY(QVector<QColor> colors READ colors WRITE setColors)
    typedef QnFramedWidget base_type;

public:
    QnControlBackgroundWidget(QGraphicsItem *parent = NULL);
    QnControlBackgroundWidget(Qt::Edge gradientBorder, QGraphicsItem *parent = NULL);

    Qt::Edge gradientBorder() const;
    void setGradientBorder(Qt::Edge gradientBorder);

    const QVector<QColor> &colors() const;
    void setColors(const QVector<QColor> &colors);

private:
    void init(Qt::Edge gradientBorder);

    void updateWindowBrush();

private:
    Qt::Edge m_gradientBorder;
    QVector<QColor> m_colors;
};
