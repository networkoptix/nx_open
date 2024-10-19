// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "texture_size_helper.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

template<> QnTextureSizeHelper* Singleton<QnTextureSizeHelper>::s_instance = nullptr;

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

                NX_MUTEX_LOCKER lock(&m_mutex);
                context->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
            },
            Qt::DirectConnection);
    }
}

int QnTextureSizeHelper::maxTextureSize() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_maxTextureSize;
}
