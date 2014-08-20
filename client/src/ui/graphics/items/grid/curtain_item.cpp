#include "curtain_item.h"

#include <limits>

#include <ui/workaround/gl_native_painting.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <utils/common/warnings.h>
#include <opengl_renderer.h>

QnCurtainItem::QnCurtainItem(QGraphicsItem *parent):
    QGraphicsObject(parent)
{
    qreal d = std::numeric_limits<qreal>::max() / 4;
    m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

    setAcceptedMouseButtons(0);
    /* Don't disable this item here or it will swallow mouse wheel events. */
}

QRectF QnCurtainItem::boundingRect() const {
    return m_boundingRect;
}

void QnCurtainItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
#ifdef Q_OS_WIN
    QRectF viewportRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
    painter->fillRect(viewportRect, m_color);
#else
    QnGlNativePainting::begin(QGLContext::currentContext(),painter);

    QMatrix4x4 m = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getModelViewMatrix();
    //glPushMatrix();

    QnOpenGLRendererManager::instance(QGLContext::currentContext())->getModelViewMatrix().setToIdentity();
    //glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    QnOpenGLRendererManager::instance(QGLContext::currentContext())->setColor(m_color);
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawColoredQuad(widget->geometry());
    /*
    glBegin(GL_QUADS);
    glColor(m_color);
    glVertices(widget->geometry());
    glEnd();*/

    glDisable(GL_BLEND); 

    QnOpenGLRendererManager::instance(QGLContext::currentContext())->getModelViewMatrix() = m;
    //glPopMatrix();
    QnGlNativePainting::end(painter);
#endif //  Q_OS_WIN
}
