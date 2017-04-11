#pragma once

#include <QtWidgets/QGraphicsProxyWidget>

/**
 * A proxy widget that can be forced to draw only part of the source widget.
 * This is useful when animating source widget's size, as constantly re-rendering
 * it into cache may be very slow.
 *
 * Later this class evolved into an optimized graphics proxy that repaints its
 * internal buffer only when embedded widget changes. The best compatibility
 * is with cacheMode == ItemCoordinateCache, but it cannot benefit from
 * our sharp pixmap painting mechanism.
 *
 * WARNING! Without caching (cacheMode == NoCache) it does not support embedded
 * widget scroll operations, unless used with patched Qt!
 */
class QnMaskedProxyWidget: public QGraphicsProxyWidget
{
    Q_OBJECT
    Q_PROPERTY(bool updatesEnabled READ isUpdatesEnabled WRITE setUpdatesEnabled)
    Q_PROPERTY(QRectF paintRect READ paintRect WRITE setPaintRect)
    Q_PROPERTY(QRectF paintGeometry READ paintGeometry WRITE setPaintGeometry)
    Q_PROPERTY(QSizeF paintSize READ paintSize WRITE setPaintSize)

    using base_type = QGraphicsProxyWidget;

public:
    QnMaskedProxyWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~QnMaskedProxyWidget();

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QRectF paintRect() const;
    void setPaintRect(const QRectF& paintRect);

    QSizeF paintSize() const;
    void setPaintSize(const QSizeF& paintSize);

    QRectF paintGeometry() const;
    void setPaintGeometry(const QRectF& paintGeometry);

    bool isUpdatesEnabled() const;

signals:
    void paintRectChanged();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool ensurePixmap(const QSize& logicalSize, int devicePixelRatio); //< returns true if pixmap was changed
    void renderWidgetRect(const QRect& logicalRect, int devicePixelRatio);
    void syncDirtyRect();

public slots:
    void setUpdatesEnabled(bool updatesEnabled);
    void enableUpdates() { setUpdatesEnabled(true); }
    void disableUpdates() { setUpdatesEnabled(false); }

private:
    QRectF m_paintRect;
    bool m_updatesEnabled;
    QPixmap m_pixmap;
    QRect m_pixmapRect;
    bool m_itemCached;
    bool m_fullRepaintPending;
    QRect m_dirtyRect;
};
