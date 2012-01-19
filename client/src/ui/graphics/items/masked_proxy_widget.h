#ifndef QN_MASKED_PROXY_WIDGET_H
#define QN_MASKED_PROXY_WIDGET_H

#include <QGraphicsProxyWidget>

class QnMaskedProxyWidget: public QGraphicsProxyWidget {
    Q_OBJECT;

    typedef QGraphicsProxyWidget base_type;

public:
    QnMaskedProxyWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual ~QnMaskedProxyWidget();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const QRectF &paintRect() const {
        return m_paintRect;
    }

    void setPaintRect(const QRectF &paintRect);

    QRectF paintGeometry() const;

    void setPaintGeometry(const QRectF &paintGeometry);

    virtual bool eventFilter(QObject *object, QEvent *event) override;

signals:
    void paintRectChanged();

private:
    QRectF m_paintRect;
    bool m_pixmapDirty;
    QPixmap m_pixmap;
};


#endif // QN_MASKED_PROXY_WIDGET_H

