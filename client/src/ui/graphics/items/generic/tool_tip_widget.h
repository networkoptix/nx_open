#ifndef QN_TOOL_TIP_WIDGET_H
#define QN_TOOL_TIP_WIDGET_H

#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtGui/QFont>

#include <ui/graphics/items/standard/graphics_widget.h>

class QnToolTipWidget: public GraphicsWidget {
    Q_OBJECT;

    typedef GraphicsWidget base_type;

public:
    QnToolTipWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnToolTipWidget();

    /* Use setContentsMargins to change tooltip's margins. */

    QPointF tailPos() const;
    void setTailPos(const QPointF &tailPos);

    const QString &text() const;
    void setText(const QString &text);

    virtual QRectF boundingRect() const override;
    virtual QPainterPath shape() const override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    void invalidateShape();
    void ensureShape() const;

private:
    mutable bool m_shapeValid;
    mutable QPainterPath m_borderShape;
    mutable QRectF m_boundingRect;

    QString m_text;
};


#endif // QN_TOOL_TIP_WIDGET_H
