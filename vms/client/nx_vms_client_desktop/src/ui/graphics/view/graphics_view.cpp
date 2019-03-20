#include "graphics_view.h"

#include <QtOpenGL/QtOpenGL>

#include <nx/utils/log/assert.h>

QnGraphicsView::QnGraphicsView(QGraphicsScene* scene, QWidget* parent):
    QGraphicsView(scene, parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
}

QnGraphicsView::~QnGraphicsView()
{
}

void QnGraphicsView::paintEvent(QPaintEvent* event)
{
    auto context = QOpenGLContext::currentContext();
    if (!context)
    {
        if (const auto glWidget = qobject_cast<QOpenGLWidget*>(viewport()))
            glWidget->makeCurrent();

        NX_ASSERT(QOpenGLContext::currentContext());
    }

    base_type::paintEvent(event);
}

void QnGraphicsView::wheelEvent(QWheelEvent* event)
{
    // Copied from QGraphicsView implementation. The only difference is QAbstractScrollArea
    // implementation is not invoked.

    if (!scene() || !isInteractive())
        return;

    event->ignore();

    QGraphicsSceneWheelEvent wheelEvent(QEvent::GraphicsSceneWheel);
    wheelEvent.setWidget(viewport());
    wheelEvent.setScenePos(mapToScene(event->pos()));
    wheelEvent.setScreenPos(event->globalPos());
    wheelEvent.setButtons(event->buttons());
    wheelEvent.setModifiers(event->modifiers());
    wheelEvent.setDelta(event->delta());
    wheelEvent.setOrientation(event->orientation());
    wheelEvent.setAccepted(false);
    QApplication::sendEvent(scene(), &wheelEvent);
    event->setAccepted(wheelEvent.isAccepted());
}
