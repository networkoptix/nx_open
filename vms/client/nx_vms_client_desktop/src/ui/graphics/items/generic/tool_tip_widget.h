// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QBrush>
#include <QtGui/QFont>
#include <QtGui/QPen>
#include <QtGui/QTextDocument>

#include "framed_widget.h"

/**
 * A generic balloon tool tip widget. It works just like any other normal widget,
 * and almost anything can be placed on it in a layout.
 *
 * Use <tt>setContentsMargins</tt> to change tool tip's content margins and rounding radius.
 */
class QnToolTipWidget: public QnFramedWidget {
    Q_OBJECT;
    typedef QnFramedWidget base_type;

public:
    QnToolTipWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {});
    virtual ~QnToolTipWidget();

    /**
     * \returns                         Position of balloon's tail, in local coordinates.
     */
    QPointF tailPos() const;

    /**
     * \param tailPos                   New position of balloon's tail, in local coordinates.
     */
    void setTailPos(const QPointF &tailPos);

    /**
     * \returns                         Widget's side to which balloon's tail is attached, or zero if
     *                                  there is no tail.
     */
    Qt::Edges tailBorder() const;

    /**
     * \returns                         Width of the base of balloon's tail.
     */
    qreal tailWidth() const;

    /**
     * \param tailWidth                 New width of the base of balloon's tail.
     */
    void setTailWidth(qreal tailWidth);

    /**
     * Moves the tooltip item so that its tail points to the given position.
     *
     * \param pos                       Position for the tooltip to point to, in parent coordinates.
     */
    void pointTo(const QPointF &pos);

    /**
     * A convenience accessor for tool tips that contain a text label (<tt>QnProxyLabel</tt>).
     *
     * \return                          Text of this tool tip's label, or <tt>null</tt> string if
     *                                  this widget does not contain a label.
     */
    QString text() const;

    /**
     * A convenience accessor that ensures that this tool tip contains a text label (<tt>QnProxyLabel</tt>)
     * and sets this label's text. Note that if some other widgets are placed on this tool tip, they
     * will be destroyed.
     *
     * \param text                      New text for this tool tip's label.
     */
    void setText(const QString &text, qreal lineHeight = 100);

    void setTextWidth(qreal width);
    void setTextColor(const QColor& color);

    virtual QRectF boundingRect() const override;

    virtual void setGeometry(const QRectF &rect) override;

    void setRoundingRadius(qreal radius);

signals:
    void tailPosChanged();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    void invalidateShape();
    void ensureShape() const;
    void updateHeight();
    void generateTextPixmap();

private:
    mutable bool m_shapeValid;
    mutable QPainterPath m_borderShape;
    mutable QRectF m_boundingRect;
    QTextDocument m_textDocument;

    QPointF m_tailPos;
    qreal m_tailWidth;
    qreal m_roundingRadius;
    bool m_autoSize;

    QString m_text;
    qreal m_textWidth = 1;
    qreal m_lineHeight = -1;
    qreal m_oldHeight = -1;
    QColor m_textColor = Qt::black;
    QPixmap m_textPixmap;
};
