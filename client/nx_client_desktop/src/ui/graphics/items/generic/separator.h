
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
    QnSeparator(qreal width
        , const QColor &color
        , QGraphicsItem *parent = nullptr);

    QnSeparator(const SeparatorAreas &areas
        , QGraphicsItem *parent = nullptr);

    virtual ~QnSeparator();

private:
    void init();

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    typedef QPair<QColor, QRectF> SeparatorRectProps;
    typedef QVector<SeparatorRectProps> SeparatorRectPropsVector;

    void updatePixmap();

private:
    const SeparatorAreas m_areas;
    QPixmap m_pixmap;
};