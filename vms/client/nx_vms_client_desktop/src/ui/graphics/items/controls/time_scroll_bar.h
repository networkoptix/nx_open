// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_TIME_SCROLL_BAR_H
#define QN_TIME_SCROLL_BAR_H

#include <qt_graphics_items/graphics_scroll_bar.h>

class QnTimeScrollBarPrivate;

class QnTimeScrollBar: public GraphicsScrollBar
{
    Q_OBJECT

    typedef GraphicsScrollBar base_type;
    using milliseconds = std::chrono::milliseconds;

public:
    QnTimeScrollBar(QGraphicsItem* parent = nullptr);
    virtual ~QnTimeScrollBar();

    milliseconds indicatorPosition() const;
    void setIndicatorPosition(milliseconds indicatorPosition);

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
