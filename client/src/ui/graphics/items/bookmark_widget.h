#ifndef QN_BOOKMARK_WIDGET_H
#define QN_BOOKMARK_WIDGET_H

#include <QGraphicsWidget>

class QnBookmarkWidget: public QGraphicsWidget {
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    enum Shape {
        RoundedNorth, /**< The normal rounded look above the pages. */
        RoundedSouth, /**< The normal rounded look below the pages. */
        RoundedWest,  /**< The normal rounded look on the left side of the pages. */
        RoundedEast,  /**< The normal rounded look on the right side the pages. */
    };

    QnBookmarkWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual ~QnBookmarkWidget();

    Shape bookmarkShape() const {
        return m_shape;
    }

    void setBookmarkShape(Shape shape);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void changeEvent(QEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    void invalidateShape();
    void ensureShape() const;

private:
    Shape m_shape;
    
    mutable bool m_shapeValid;
    mutable QPainterPath m_borderShape;
};

#endif // QN_BOOKMARK_WIDGET_H
