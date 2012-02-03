#include "caching_proxy_widget.h"
#include <cassert>
#include <QEvent>
#include <QSysInfo>
#include <ui/common/opengl.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/common/scene_utility.h>

namespace {

    int checkOpenGLError() {
        int error = glGetError();
        if (error != GL_NO_ERROR) {
            const char *errorString = reinterpret_cast<const char *>(gluErrorString(error));
            if (errorString)
                qnWarning("OpenGL Error: %1.", errorString);
        }
        return error;
    }

} // anonymous namespace

CachingProxyWidget::CachingProxyWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_maxTextureSize(maxTextureSize()),
    m_texture(0),
    m_wholeTextureDirty(true)
{}

CachingProxyWidget::~CachingProxyWidget() {
    if(m_texture != 0)
        glDeleteTextures(1, &m_texture);
}

void CachingProxyWidget::ensureTextureAllocated() {
    if(m_texture != 0)
        return;

    glGenTextures(1, &m_texture);

    /* Set texture parameters. */
    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPopAttrib();
}

void CachingProxyWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) {
    if (painter->paintEngine() == NULL) {
        qnWarning("No OpenGL-compatible paint engine was found.");
        return;
    }

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL2 && painter->paintEngine()->type() != QPaintEngine::OpenGL) {
        qnWarning("Painting with the paint engine of type '%1' is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }

    ensureTextureAllocated();
    
    ensureCurrentWidgetSynchronized();
    if(currentWidget() == NULL || !currentWidget()->isVisible())
        return;

    const QRect exposedWidgetRect = (option->exposedRect & rect()).toAlignedRect();
    if (exposedWidgetRect.isEmpty())
        return;

    painter->beginNativePainting();
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);

    ensureTextureSynchronized();

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); /* For opacity to work. */

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QRectF vertexRect = this->rect();
    QRectF textureRect = QRectF(
        QPointF(0.0, 0.0),
        SceneUtility::cwiseDiv(vertexRect.size(), m_image.size())
    );
    glBegin(GL_QUADS);
    glColor(1.0, 1.0, 1.0, effectiveOpacity());
    glTexCoord(textureRect.topLeft());
    glVertex(vertexRect.topLeft());
    glTexCoord(textureRect.topRight());
    glVertex(vertexRect.topRight());
    glTexCoord(textureRect.bottomRight());
    glVertex(vertexRect.bottomRight());
    glTexCoord(textureRect.bottomLeft());
    glVertex(vertexRect.bottomLeft());
    glEnd();

    glPopAttrib();
    painter->endNativePainting();
}

bool CachingProxyWidget::eventFilter(QObject *object, QEvent *event) {
    if(event->type() == QEvent::UpdateRequest && object == currentWidget()) {
        m_wholeTextureDirty = true;
    }
    
    return base_type::eventFilter(object, event);
}

int CachingProxyWidget::maxTextureSize() {
    GLint result;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &result);
    return result;
}

void CachingProxyWidget::ensureCurrentWidgetSynchronized() {
    if(currentWidget() == widget())
        return;

    m_wholeTextureDirty = true;

    if(currentWidget() != NULL)
        currentWidget()->removeEventFilter(this);

    m_currentWidget = widget();

    if(currentWidget() != NULL)
        currentWidget()->installEventFilter(this);
}

void CachingProxyWidget::ensureTextureSizeSynchronized() {
    assert(currentWidget() != NULL);

    if(SceneUtility::contains(m_image.size(), currentWidget()->size()))
        return;

    if(m_image.width() == m_maxTextureSize)
        return;

    int widgetSide = qMax(currentWidget()->width(), currentWidget()->height());
    int textureSide = 256;
    for(; textureSide <= m_maxTextureSize; textureSide *= 2)
        if(textureSide >= widgetSide)
            break;
    textureSide = qMin(textureSide, m_maxTextureSize);

    m_image = QImage(textureSide, textureSide, QImage::Format_ARGB32);
    m_wholeTextureDirty = true;
}

void CachingProxyWidget::ensureTextureSynchronized() {
    assert(currentWidget() != NULL);

    ensureTextureSizeSynchronized();

    if(!m_wholeTextureDirty)
        return;
    
    currentWidget()->render(&m_image, QPoint(0, 0), currentWidget()->rect());

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_image.bytesPerLine() * 8 / m_image.depth());

    /* QImage's Format_ARGB32 corresponds to OpenGL's GL_BGRA on little endian architectures.
     * On big endian architectures, conversion is needed. */
    assert(QSysInfo::ByteOrder == QSysInfo::LittleEndian);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_image.width(), m_image.height(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_image.bits());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    /* No need to restore OpenGL state here, it is done by the caller. */

    m_wholeTextureDirty = false;
}


/*

glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);
glTexSubImage2D(GL_TEXTURE_2D, 0,
    0, 0,
    roundUp(r_w[i],ROUND_COEFF),
    h[i],
    GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

*/