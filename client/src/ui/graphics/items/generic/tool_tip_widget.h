#ifndef QN_TOOL_TIP_WIDGET_H
#define QN_TOOL_TIP_WIDGET_H

#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtGui/QFont>

#include "simple_frame_widget.h"

class QnToolTipWidget: public QnSimpleFrameWidget {
    Q_OBJECT;
    typedef QnSimpleFrameWidget base_type;

public:
    QnToolTipWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnToolTipWidget();

    /* Use setContentsMargins to change tooltip's margins. */

    /**
     * \returns                         Position of balloon's tail, in local coordinates.
     */
    QPointF tailPos() const;

    /**
     * \returns                         Widget's side to which balloon's tail is attached, or zero if 
     *                                  there is no tail.
     */
    Qn::Border tailBorder() const;

    /**
     * \param tailPos                   New position of balloon's tail, in local coordinates.
     */
    void setTailPos(const QPointF &tailPos);

    /**
     * \returns                         Width of the base of balloon's tail.
     */
    qreal tailWidth() const;

    /**
     * \param tailWidth                 New width of the base of balloon's tail.
     */
    void setTailWidth(qreal tailWidth);

    void pointTo(const QPointF &pos);


    QString text() const;
    void setText(const QString &text);

    virtual QRectF boundingRect() const override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    void invalidateShape();
    void ensureShape() const;

private:
    mutable bool m_shapeValid;
    mutable QPainterPath m_borderShape;
    mutable QRectF m_boundingRect;

    QPointF m_tailPos;
    qreal m_tailWidth;
};


#endif // QN_TOOL_TIP_WIDGET_H
