// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "graphics_view.h"

#include <QtGui/QOpenGLContext>
#include <QtWidgets/QApplication>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QGraphicsSceneWheelEvent>
#include <QtGui/QWheelEvent>

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/trace/trace.h>

using namespace nx::vms::client::desktop;

QnGraphicsView::QnGraphicsView(QGraphicsScene* scene, QWidget* parent):
    QGraphicsView(scene, parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    setPaletteColor(this, QPalette::Base, colorTheme()->color("dark1"));
}

QnGraphicsView::~QnGraphicsView()
{
}

void QnGraphicsView::paintEvent(QPaintEvent* event)
{
    NX_TRACE_SCOPE("paintEvent");

    // Always make QOpenGLWidget context to be the current context.
    // This is what item paint functions expect and doing otherwise
    // may lead to unpredictable behavior.
    if (const auto glWidget = qobject_cast<QOpenGLWidget*>(viewport()))
        glWidget->makeCurrent();

    NX_ASSERT(QOpenGLContext::currentContext());

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
    wheelEvent.setScenePos(mapToScene(event->position().toPoint()));
    wheelEvent.setScreenPos(event->globalPosition().toPoint());
    wheelEvent.setButtons(event->buttons());
    wheelEvent.setModifiers(event->modifiers());
    const auto angleDelta = event->angleDelta();
    if (angleDelta.y() != 0)
    {
        wheelEvent.setDelta(angleDelta.y());
        wheelEvent.setOrientation(Qt::Vertical);
    }
    else
    {
        wheelEvent.setDelta(angleDelta.x());
        wheelEvent.setOrientation(Qt::Horizontal);
    }
    wheelEvent.setPhase(event->phase());
    wheelEvent.setInverted(event->isInverted());
    wheelEvent.setAccepted(false);
    QApplication::sendEvent(scene(), &wheelEvent);
    event->setAccepted(wheelEvent.isAccepted());
}

void QnGraphicsView::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
        setBackgroundBrush(palette().base());
}
