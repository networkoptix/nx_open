#ifndef QN_TEXT_BUTTON_WIDGET_H
#define QN_TEXT_BUTTON_WIDGET_H

#include "image_button_widget.h"
#include "framed_widget.h"

/**
 * An image button widget that can show text and draw a frame.
 */
class QnTextButtonWidget: public Framed<QnImageButtonWidget> {
    Q_OBJECT
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(Qn::FrameShape frameShape READ frameShape WRITE setFrameShape)
    Q_PROPERTY(Qt::PenStyle frameStyle READ frameStyle WRITE setFrameStyle)
    Q_PROPERTY(QBrush frameBrush READ frameBrush WRITE setFrameBrush)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QBrush windowBrush READ windowBrush WRITE setWindowBrush)
    Q_PROPERTY(QColor windowColor READ windowColor WRITE setWindowColor)
    Q_PROPERTY(QBrush textBrush READ textBrush WRITE setTextBrush)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(qreal relativeFrameWidth READ relativeFrameWidth WRITE setRelativeFrameWidth)
    typedef Framed<QnImageButtonWidget> base_type;

public:
    QnTextButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    const QString &text() const;
    void setText(const QString &text);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QBrush textBrush() const;
    void setTextBrush(const QBrush &textBrush);

    QColor textColor() const;
    void setTextColor(const QColor &textColor);

    qreal relativeFrameWidth() const;
    void setRelativeFrameWidth(qreal relativeFrameWidth);

    qreal stateOpacity(StateFlags stateFlags) const;
    void setStateOpacity(StateFlags stateFlags, qreal opacity);

    virtual void setGeometry(const QRectF &geometry) override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) override;

    virtual void changeEvent(QEvent *event) override;

    void invalidatePixmap();
    void ensurePixmap();

protected:
    StateFlags validOpacityState(StateFlags flags) const;

private:
    QString m_text;
    Qt::Alignment m_alignment;
    bool m_pixmapValid;
    qreal m_relativeFrameWidth;
    boost::array<qreal, MaxState + 1> m_opacities;
};

#endif // QN_TEXT_BUTTON_WIDGET_H
