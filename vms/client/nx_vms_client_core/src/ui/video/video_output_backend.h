#pragma once

#include <QtQuick/QQuickItem>
#include <QtMultimedia/QAbstractVideoSurface>

class QVideoRendererControl;
class QOpenGLContext;
class QSGVideoNodeFactoryInterface;

class QnVideoOutput;
class QnVideoOutputBackendPrivate;

class QnVideoOutputBackend
{
public:
    QnVideoOutputBackend(QnVideoOutput* videoOutput);
    ~QnVideoOutputBackend();

    void releasePlayer();
    QSize nativeSize() const;
    void updateGeometry();
    QSGNode* updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* data);
    QAbstractVideoSurface* videoSurface() const;
    QRectF adjustedViewport() const;
    QOpenGLContext* glContext() const;

    void present(const QVideoFrame& frame);
    void stop();

    QList<QSGVideoNodeFactoryInterface*> videoNodeFactories() const;

private:
    void scheduleDeleteFilterResources();

    QScopedPointer<QnVideoOutputBackendPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnVideoOutputBackend)
};
