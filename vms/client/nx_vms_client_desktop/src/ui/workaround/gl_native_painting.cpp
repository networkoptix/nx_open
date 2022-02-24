// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_native_painting.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QOpenGLFunctions>

#include "opengl_renderer.h"

void QnGlNativePainting::begin(QOpenGLWidget* glWidget, QPainter* painter)
{
    painter->beginNativePainting();

    QPaintEngine *engine = painter->paintEngine();
    QPaintEngine::Type engineType = engine ? engine->type() : QPaintEngine::User;

    if(engineType == QPaintEngine::OpenGL2 || engineType == QPaintEngine::OpenGL) {
        /* There is a bug in Qt that causes QPainter::beginNativePainting not to
         * set up the transformation properly in some cases. Working it around
         * by using QGLFormat::CompatibilityProfile has resulted in black
         * main window on some machines, so for now we just work it around by
         * copying code from qpaintengineex_opengl2.cpp. Executing this code
         * twice will not result in any problems, so we don't do any
         * additional checks. */
        const auto aspect = painter->device()->devicePixelRatio();
        const QSize sz = QSize(painter->device()->width() * aspect
            , painter->device()->height() * aspect);
        QMatrix4x4 m;
        m.ortho(0, sz.width(), sz.height(), 0, -999999, 999999);
        QnOpenGLRendererManager::instance(glWidget)->setProjectionMatrix(m);
        QnOpenGLRendererManager::instance(glWidget)->setModelViewMatrix(QMatrix4x4(painter->deviceTransform()));

        if (painter->hasClipping())
        {
            /* This reconfigures OpenGL clipping that was reset in beginNativePainting: */
            painter->setClipping(false);
            painter->setClipping(true);
        }
    }
}

void QnGlNativePainting::end(QPainter *painter)
{
    painter->endNativePainting();
}
