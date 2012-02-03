#include "curtain_item.h"
#include <limits>
#include <ui/common/opengl.h>

QnCurtainItem::QnCurtainItem(QGraphicsItem *parent):
    QGraphicsObject(parent)
{
    qreal d = std::numeric_limits<qreal>::max() / 4;
    m_boundingRect = QRectF(QPointF(-d, -d), QPoint(d, d));

    setAcceptedMouseButtons(0);
    /* Don't disable this item here or it will swallow mouse wheel events. */
}

QRectF QnCurtainItem::boundingRect() const {
    return m_boundingRect;
}

void QnCurtainItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    painter->beginNativePainting();
    glPushMatrix();
    glLoadIdentity();

    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    glBegin(GL_QUADS);
    glColor(m_color);
    glVertices(widget->geometry());
    glEnd();

    glPopAttrib();
    glPopMatrix();
    painter->endNativePainting();
}
