#include "gl_native_painting.h"
#include <QtGui/QOpenGLFunctions>
#include "qopenglfunctions.h"
#include "opengl_renderer.h"
void QnGlNativePainting::begin(const QGLContext* context,QPainter *painter) {
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

        /*
        const QTransform& mtx = painter->deviceTransform(); // state->matrix;

        float mv_matrix[4][4] =
        {
            { float(mtx.m11()), float(mtx.m12()),     0, float(mtx.m13()) },
            { float(mtx.m21()), float(mtx.m22()),     0, float(mtx.m23()) },
            {                0,                0,     1,                0 },
            {  float(mtx.dx()),  float(mtx.dy()),     0, float(mtx.m33()) }
        };*/

        //qDebug()<<"transform"<<mtx;
        const QSize sz = QSize(painter->device()->width(), painter->device()->height());
        QnOpenGLRendererManager::instance(context).getProjectionMatrix().setToIdentity();
        //QRectF rect(QPointF(0.0f,0.0f),sz);
        //QnOpenGLRendererManager::instance(context).getProjectionMatrix().ortho(rect);
        QnOpenGLRendererManager::instance(context).getProjectionMatrix().ortho(0, sz.width(), sz.height(), 0, -999999, 999999);

        QnOpenGLRendererManager::instance(context).getModelViewMatrix() = QMatrix4x4(painter->deviceTransform());

        /*
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, sz.width(), sz.height(), 0, -999999, 999999);

        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(&mv_matrix[0][0]);*/

    }
}

void QnGlNativePainting::end(QPainter *painter) {
    painter->endNativePainting();
}
