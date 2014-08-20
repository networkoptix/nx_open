#ifndef QN_CACHING_PROXY_WIDGET_H
#define QN_CACHING_PROXY_WIDGET_H

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtGui/QImage>

/**
 * Proxy widget that caches the widget's surface.
 * 
 * Currently unused as <tt>QGraphicsItem::setCacheMode</tt> can be used to
 * achieve the same result.
 */
class CachingProxyWidget: public QGraphicsProxyWidget {
    Q_OBJECT;

    typedef QGraphicsProxyWidget base_type;

public:
    CachingProxyWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual ~CachingProxyWidget();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    void ensureTextureAllocated();
    void ensureTextureSynchronized();
    void ensureTextureSizeSynchronized();
    void ensureCurrentWidgetSynchronized();

    QWidget *currentWidget() const {
        return m_currentWidget.data();
    }

private:
    QPointer<QWidget> m_currentWidget;
    int m_maxTextureSize;
    QPoint m_offset;
    QImage m_image;
    unsigned m_texture;
    bool m_wholeTextureDirty;

    bool m_initialized;
    QOpenGLVertexArrayObject m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_textureBuffer;
};


#endif // QN_CACHING_PROXY_WIDGET_H
