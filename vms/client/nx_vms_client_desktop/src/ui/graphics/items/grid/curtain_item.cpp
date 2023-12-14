// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "curtain_item.h"

#include <limits>

#include <QtGui/QPainter>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/workaround/gl_native_painting.h>

using namespace nx::vms::client::core;

QnCurtainItem::QnCurtainItem(QGraphicsItem* parent):
    base_type(parent),
    m_color(colorTheme()->color("scene.curtain")),
    m_boundingRect(Geometry::maxBoundingRect())
{
    setAcceptedMouseButtons(Qt::NoButton);
    // Don't disable this item here or it will swallow mouse wheel events.
}

QRectF QnCurtainItem::boundingRect() const
{
    return m_boundingRect;
}

void QnCurtainItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
    const auto glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget)
    {
        if (!scene()->views().empty())
        {
            widget = scene()->views().front()->viewport();
            QRectF viewportRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
            painter->fillRect(viewportRect, m_color);
        }
        return;
    }
#ifdef Q_OS_WIN
    QRectF viewportRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
    painter->fillRect(viewportRect, m_color);
#else
    QnGlNativePainting::begin(glWidget, painter);

    const auto renderer = QnOpenGLRendererManager::instance(glWidget);
    renderer->pushModelViewMatrix();
    renderer->setModelViewMatrix(QMatrix4x4());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto widgetRect = widget->geometry();
    const qreal ratio = painter->device()->devicePixelRatio();
    const auto rect = QRectF(widgetRect.topLeft() * ratio, widgetRect.size() * ratio);
    renderer->setColor(m_color);
    renderer->drawColoredQuad(rect);

    glDisable(GL_BLEND);

    renderer->popModelViewMatrix();

    QnGlNativePainting::end(painter);
#endif //  Q_OS_WIN
}
