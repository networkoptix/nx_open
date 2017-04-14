#include "curtain_item.h"

#include <limits>

#include <QtOpenGL/QGLContext>

#include <ui/workaround/gl_native_painting.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <utils/common/warnings.h>
#include <opengl_renderer.h>

QnCurtainItem::QnCurtainItem(QGraphicsItem *parent):
    base_type(parent)
{
    qreal d = std::numeric_limits<qreal>::max() / 4;
    m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

    setAcceptedMouseButtons(0);
    /* Don't disable this item here or it will swallow mouse wheel events. */
}

QRectF QnCurtainItem::boundingRect() const {
    return m_boundingRect;
}

void QnCurtainItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
#ifdef Q_OS_WIN
    QRectF viewportRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
    painter->fillRect(viewportRect, m_color);
#else
    QnGlNativePainting::begin(QGLContext::currentContext(), painter);

    QnOpenGLRendererManager::instance(QGLContext::currentContext())->pushModelViewMatrix();
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->setModelViewMatrix(QMatrix4x4());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto widgetRect = widget->geometry();
    const qreal ratio = painter->device()->devicePixelRatio();
    const auto rect = QRectF(widgetRect.topLeft() * ratio, widgetRect.size() * ratio);
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->setColor(m_color);
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawColoredQuad(rect);

    glDisable(GL_BLEND);

    QnOpenGLRendererManager::instance(QGLContext::currentContext())->popModelViewMatrix();

    QnGlNativePainting::end(painter);
#endif //  Q_OS_WIN
}

const QColor & QnCurtainItem::color() const
{
    return m_color;
}

void QnCurtainItem::setColor(const QColor &color)
{
    m_color = color;
}
