#ifndef QN_MASKED_PROXY_WIDGET_H
#define QN_MASKED_PROXY_WIDGET_H

#include <QtWidgets/QGraphicsProxyWidget>

/**
 * A proxy widget that can be forced to draw only part of the source widget.
 * This is useful when animating source widget's size, as constantly re-rendering
 * it into cache may be very slow.
 */
class QnMaskedProxyWidget: public QGraphicsProxyWidget {
    Q_OBJECT;
    Q_PROPERTY(bool updatesEnabled READ isUpdatesEnabled WRITE setUpdatesEnabled);
    Q_PROPERTY(QRectF paintRect READ paintRect WRITE setPaintRect);
    Q_PROPERTY(QRectF paintGeometry READ paintGeometry WRITE setPaintGeometry);
    Q_PROPERTY(QSizeF paintSize READ paintSize WRITE setPaintSize);

    typedef QGraphicsProxyWidget base_type;

public:
    QnMaskedProxyWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual ~QnMaskedProxyWidget();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QRectF paintRect() const;
    void setPaintRect(const QRectF &paintRect);

    QSizeF paintSize() const;
    void setPaintSize(const QSizeF &paintSize);

    QRectF paintGeometry() const;
    void setPaintGeometry(const QRectF &paintGeometry);

    virtual bool eventFilter(QObject *object, QEvent *event) override;

    bool isUpdatesEnabled() const;

    virtual bool event( QEvent* e ) override;

signals:
    void paintRectChanged();

public slots:
    void setUpdatesEnabled(bool updatesEnabled);
    void enableUpdates() { setUpdatesEnabled(true); }
    void disableUpdates() { setUpdatesEnabled(false); }

private:
    QRectF m_paintRect;
    bool m_pixmapDirty;
    bool m_updatesEnabled;
    QPixmap m_pixmap;
};


#endif // QN_MASKED_PROXY_WIDGET_H

