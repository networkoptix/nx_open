/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "videonode_rgb.h"
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgmaterial.h>
#include <QtCore/qmutex.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>

#include <nx/utils/log/log.h>

#include <nx/media/quick_sync/quick_sync_surface.h>
#include <nx/media/quick_sync/quick_sync_video_decoder_impl.h>

QT_BEGIN_NAMESPACE

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_RGB::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> pixelFormats;

    if (handleType == QAbstractVideoBuffer::NoHandle) {
        pixelFormats.append(QVideoFrame::Format_RGB32);
        pixelFormats.append(QVideoFrame::Format_ARGB32);
        pixelFormats.append(QVideoFrame::Format_BGR32);
        pixelFormats.append(QVideoFrame::Format_BGRA32);
        pixelFormats.append(QVideoFrame::Format_RGB565);
    } else if (handleType == QAbstractVideoBuffer::GLTextureHandle) {
        pixelFormats.append(QVideoFrame::Format_NV12);
    } else if (handleType == kHandleTypeQsvSurface) {
        pixelFormats.append(QVideoFrame::Format_NV12);
    }

    return pixelFormats;
}

QSGVideoNode *QSGVideoNodeFactory_RGB::createNode(const QVideoSurfaceFormat &format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QSGVideoNode_RGB(format);

    return 0;
}


class QSGVideoMaterialShader_RGB : public QSGMaterialShader
{
public:
    QSGVideoMaterialShader_RGB()
        : QSGMaterialShader(),
          m_id_matrix(-1),
          m_id_width(-1),
          m_id_rgbTexture(-1),
          m_id_opacity(-1)
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qtmultimediaquicktools/shaders/rgbvideo_padded.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/rgbvideo.frag"));
    }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

    virtual char const *const *attributeNames() const {
        static const char *names[] = {
            "qt_VertexPosition",
            "qt_VertexTexCoord",
            0
        };
        return names;
    }

protected:
    virtual void initialize() {
        m_id_matrix = program()->uniformLocation("qt_Matrix");
        m_id_width = program()->uniformLocation("width");
        m_id_rgbTexture = program()->uniformLocation("rgbTexture");
        m_id_opacity = program()->uniformLocation("opacity");
    }

    int m_id_matrix;
    int m_id_width;
    int m_id_rgbTexture;
    int m_id_opacity;
};

class QSGVideoMaterialShader_RGB_swizzle : public QSGVideoMaterialShader_RGB
{
public:
    QSGVideoMaterialShader_RGB_swizzle()
        : QSGVideoMaterialShader_RGB()
    {
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/rgbvideo_swizzle.frag"));
    }
};


class QSGVideoMaterial_RGB : public QSGMaterial
{
public:
    QSGVideoMaterial_RGB(const QVideoSurfaceFormat &format) :
        m_format(format),
        m_textureId(0),
        m_opacity(1.0),
        m_width(1.0)
    {
        setFlag(Blending, false);
    }

    ~QSGVideoMaterial_RGB()
    {
        if (m_textureId)
            QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_textureId);
    }

    virtual QSGMaterialType *type() const {
        static QSGMaterialType normalType, swizzleType;
        return needsSwizzling() ? &swizzleType : &normalType;
    }

    virtual QSGMaterialShader *createShader() const {
        return needsSwizzling() ? new QSGVideoMaterialShader_RGB_swizzle
                                : new QSGVideoMaterialShader_RGB;
    }

    virtual int compare(const QSGMaterial *other) const {
        const QSGVideoMaterial_RGB *m = static_cast<const QSGVideoMaterial_RGB *>(other);

        if (!m_textureId)
            return 1;

        return m_textureId - m->m_textureId;
    }

    void updateBlending() {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void setVideoFrame(const QVideoFrame &frame) {
        QMutexLocker lock(&m_frameMutex);
        m_frame = frame;
        m_textureDirty = true;
    }

    void renderQsvSurface()
    {
        QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
        bool isNewTexture = false;
        if (m_textureSize != m_frame.size()) {
            if (!m_textureSize.isEmpty())
                functions->glDeleteTextures(1, &m_textureId);
            functions->glGenTextures(1, &m_textureId);
            isNewTexture = true;
            // TODO make texture size equal to rendering rect
            m_textureSize = m_frame.size();

            functions->glActiveTexture(GL_TEXTURE0);
            functions->glBindTexture(GL_TEXTURE_2D, m_textureId);
            functions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                m_textureSize.width(), m_textureSize.height(), 0,
                GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        auto surfaceInfo = m_frame.handle().value<QuickSyncSurface>();
        auto decoderLock = surfaceInfo.decoder.lock();
        if (!decoderLock)
            return;

        if (!decoderLock->getDevice().renderToRgb(surfaceInfo, isNewTexture, m_textureId, nullptr /*TODO support windows*/))
            NX_WARNING(this, "rendering surface failed");

        m_textureDirty = false;
        functions->glActiveTexture(GL_TEXTURE0);
        functions->glBindTexture(GL_TEXTURE_2D, m_textureId);
    }

    void renderSysMemory()
    {
        QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
        if (m_frame.map(QAbstractVideoBuffer::ReadOnly)) {
            QSize textureSize = m_frame.size();

            int stride = m_frame.bytesPerLine();
            switch (m_frame.pixelFormat()) {
            case QVideoFrame::Format_RGB565:
                stride /= 2;
                break;
            default:
                stride /= 4;
            }

            m_width = qreal(m_frame.width()) / stride;
            textureSize.setWidth(stride);

            if (m_textureSize != textureSize) {
                if (!m_textureSize.isEmpty())
                    functions->glDeleteTextures(1, &m_textureId);
                functions->glGenTextures(1, &m_textureId);
                m_textureSize = textureSize;
            }

            GLint dataType = GL_UNSIGNED_BYTE;
            GLint dataFormat = GL_RGBA;

            if (m_frame.pixelFormat() == QVideoFrame::Format_RGB565) {
                dataType = GL_UNSIGNED_SHORT_5_6_5;
                dataFormat = GL_RGB;
            }

            GLint previousAlignment;
            functions->glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            functions->glActiveTexture(GL_TEXTURE0);
            functions->glBindTexture(GL_TEXTURE_2D, m_textureId);
            functions->glTexImage2D(GL_TEXTURE_2D, 0, dataFormat,
                                    m_textureSize.width(), m_textureSize.height(),
                                    0, dataFormat, dataType, m_frame.bits());

            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_frame.unmap();
        }
        m_frame = QVideoFrame();
    }

    void bind()
    {
        QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();

        QMutexLocker lock(&m_frameMutex);
        if (!m_frame.isValid() || (!m_textureDirty && m_frame.handleType() == kHandleTypeQsvSurface))
        {
            functions->glActiveTexture(GL_TEXTURE0);
            functions->glBindTexture(GL_TEXTURE_2D, m_textureId);
            return;
        }

        if (m_frame.handleType() == kHandleTypeQsvSurface)
            renderQsvSurface();
        else
            renderSysMemory();
    }

    QVideoFrame m_frame;
    QMutex m_frameMutex;
    QSize m_textureSize;
    bool m_textureDirty = true;
    QVideoSurfaceFormat m_format;
    GLuint m_textureId;
    qreal m_opacity;
    GLfloat m_width;

private:
    bool needsSwizzling() const {
        return m_format.pixelFormat() == QVideoFrame::Format_RGB32
                || m_format.pixelFormat() == QVideoFrame::Format_ARGB32;
    }
};


QSGVideoNode_RGB::QSGVideoNode_RGB(const QVideoSurfaceFormat &format) :
    m_format(format)
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QSGVideoMaterial_RGB(format);
    setMaterial(m_material);
}

QSGVideoNode_RGB::~QSGVideoNode_RGB()
{
}

QAbstractVideoBuffer::HandleType QSGVideoNode_RGB::handleType() const
{
    return m_handleType;
}

void QSGVideoNode_RGB::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    m_handleType = frame.handleType();
    m_material->setVideoFrame(frame);
    markDirty(DirtyMaterial);
}

void QSGVideoMaterialShader_RGB::updateState(const RenderState &state,
                                                QSGMaterial *newMaterial,
                                                QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);
    QSGVideoMaterial_RGB *mat = static_cast<QSGVideoMaterial_RGB *>(newMaterial);
    program()->setUniformValue(m_id_rgbTexture, 0);

    mat->bind();

    program()->setUniformValue(m_id_width, mat->m_width);
    if (state.isOpacityDirty()) {
        mat->m_opacity = state.opacity();
        mat->updateBlending();
        program()->setUniformValue(m_id_opacity, GLfloat(mat->m_opacity));
    }

    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}

QT_END_NAMESPACE
