#include "texture_size_helper.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

QnTextureSizeHelper::QnTextureSizeHelper(QQuickWindow *window, QObject *parent)
:
    QObject(parent),
    m_maxTextureSize(4096)
{
    if (window)
    {
        connect(window, &QQuickWindow::beforeSynchronizing, this,
            [this, window]()
            {
                disconnect(window, nullptr, this, nullptr);

                QOpenGLContext *context = QOpenGLContext::currentContext();
                if (!context)
                    return;

                QnMutexLocker lock(&m_mutex);
                context->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
            },
            Qt::DirectConnection);
    }
}

int QnTextureSizeHelper::maxTextureSize() const
{
    QnMutexLocker lock(&m_mutex);
    return m_maxTextureSize;
}
