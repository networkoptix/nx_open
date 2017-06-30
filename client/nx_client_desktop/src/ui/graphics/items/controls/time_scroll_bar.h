#ifndef QN_TIME_SCROLL_BAR_H
#define QN_TIME_SCROLL_BAR_H

#include <client/client_color_types.h>
#include <ui/graphics/items/standard/graphics_scroll_bar.h>

class QnTimeScrollBarPrivate;

class QnTimeScrollBar: public GraphicsScrollBar
{
    Q_OBJECT
    typedef GraphicsScrollBar base_type;

public:
    QnTimeScrollBar(QGraphicsItem* parent = nullptr);
    virtual ~QnTimeScrollBar();

    qint64 indicatorPosition() const;
    void setIndicatorPosition(qint64 indicatorPosition);

    bool indicatorVisible() const;
    void setIndicatorVisible(bool value);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

protected:
    void ensurePixmap(int devicePixelRatio);

private:
    qint64 m_indicatorPosition;
    bool m_indicatorVisible;

    QPixmap m_pixmap;

private:
    Q_DECLARE_PRIVATE(QnTimeScrollBar)
};


#endif // QN_TIME_SCROLL_BAR_H
