#ifndef QN_TIME_SCROLL_BAR_H
#define QN_TIME_SCROLL_BAR_H

#include <client/client_color_types.h>
#include <ui/graphics/items/standard/graphics_scroll_bar.h>

class QnTimeScrollBar: public GraphicsScrollBar {
    Q_OBJECT
    Q_PROPERTY(QnTimeScrollBarColors colors READ colors WRITE setColors)
    typedef GraphicsScrollBar base_type;

public:
    QnTimeScrollBar(QGraphicsItem *parent = NULL);
    virtual ~QnTimeScrollBar();

    const QnTimeScrollBarColors &colors() const;
    void setColors(const QnTimeScrollBarColors &colors);

    qint64 indicatorPosition() const;
    void setIndicatorPosition(qint64 indicatorPosition);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    QnTimeScrollBarColors m_colors;
    qint64 m_indicatorPosition;
};


#endif // QN_TIME_SCROLL_BAR_H
