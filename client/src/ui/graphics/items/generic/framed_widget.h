#ifndef QN_FRAMED_WIDGET_H
#define QN_FRAMED_WIDGET_H

#include <utils/common/forward.h>

#include <ui/graphics/items/standard/graphics_widget.h>


namespace Qn {
    enum FrameShape {
        NoFrame,
        RectangularFrame,
        EllipticalFrame,
    };
}

class FramedBase {
public:
    FramedBase();
    virtual ~FramedBase();

    qreal frameWidth() const;
    void setFrameWidth(qreal frameWidth);

    Qn::FrameShape frameShape() const;
    void setFrameShape(Qn::FrameShape frameShape);

    Qt::PenStyle frameStyle() const;
    void setFrameStyle(Qt::PenStyle frameStyle);

    QBrush frameBrush() const;
    void setFrameBrush(const QBrush &frameBrush);

    QColor frameColor() const;
    void setFrameColor(const QColor &frameColor);

    QBrush windowBrush() const;
    void setWindowBrush(const QBrush &windowBrush);

    QColor windowColor() const;
    void setWindowColor(const QColor &windowColor);

protected:
    void paintFrame(QPainter *painter, const QRectF &rect);

    void initSelf(QGraphicsWidget *self);

private:
    QGraphicsWidget *m_self;
    qreal m_frameWidth;
    Qt::PenStyle m_frameStyle;
    Qn::FrameShape m_frameShape;
};


template<class Base>
class Framed: public Base, public FramedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Framed, Base, { FramedBase::initSelf(this); })

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        FramedBase::paintFrame(painter, Base::rect());
        Base::paint(painter, option, widget);
    }
};


/**
 * A graphics frame widget that does not use style for painting.
 * 
 * Frame width and color are configurable.
 */
class QnFramedWidget: public Framed<GraphicsWidget> {
    Q_OBJECT
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(Qn::FrameShape frameShape READ frameShape WRITE setFrameShape)
    Q_PROPERTY(Qt::PenStyle frameStyle READ frameStyle WRITE setFrameStyle)
    Q_PROPERTY(QBrush frameBrush READ frameBrush WRITE setFrameBrush)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QBrush windowBrush READ windowBrush WRITE setWindowBrush)
    Q_PROPERTY(QColor windowColor READ windowColor WRITE setWindowColor)
    typedef Framed<GraphicsWidget> base_type;

public:
    QnFramedWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): base_type(parent, windowFlags) {}
};



#endif // QN_FRAMED_WIDGET_H
