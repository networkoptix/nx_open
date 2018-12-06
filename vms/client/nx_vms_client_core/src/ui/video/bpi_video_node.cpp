#include "bpi_video_node.h"

#include <QtCore/qmutex.h>
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgmaterial.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>

namespace {

const QVideoFrame::PixelFormat Format_Tiled32x32NV12 =
    QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);

} // namespace

QList<QVideoFrame::PixelFormat> QnBpiSGVideoNodeFactory_YUV::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> pixelFormats;

    if (handleType == QAbstractVideoBuffer::NoHandle)
        pixelFormats.append(Format_Tiled32x32NV12);

    return pixelFormats;
}

QSGVideoNode* QnBpiSGVideoNodeFactory_YUV::createNode(const QVideoSurfaceFormat& format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QnBpiSGVideoNode_YUV(format);

    return 0;
}

//-------------------------------------------------------------------------------------------------

namespace {

int qPower2Ceil(int value, int step)
{
    return ((value - 1) & ~(step - 1)) + step;
}

class QSGVideoMaterialShader_YUV_BiPlanarTiled: public QSGMaterialShader
{
public:
    QSGVideoMaterialShader_YUV_BiPlanarTiled(): QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(
            ":/shaders/tilednv12_32x32_video.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(
            ":/shaders/tilednv12_32x32_video.frag"));
    }

    virtual void updateState(
        const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial);

    virtual const char* const* attributeNames() const
    {
        static const char* const names[] = {
            "qt_VertexPosition",
            "qt_VertexTexCoord",
            0};
        return names;
    }

protected:
    virtual void initialize()
    {
        m_id_matrix = program()->uniformLocation("qt_Matrix");

        m_id_planeWidth = program()->uniformLocation("planeWidth");
        m_id_planeHeight = program()->uniformLocation("planeHeight");
        m_id_yTextureHeight = program()->uniformLocation("yTextureHeight");

        m_id_plane1Texture = program()->uniformLocation("plane1Texture");
        m_id_plane2Texture = program()->uniformLocation("plane2Texture");
        m_id_colorMatrix = program()->uniformLocation("colorMatrix");
        m_id_opacity = program()->uniformLocation("opacity");

        m_id_uvBlocksCount = program()->uniformLocation("uvBlocksCount");
        m_id_yBlocksPerLine = program()->uniformLocation("yBlocksPerLine");
    }

    int m_id_matrix;
    int m_id_planeWidth;
    int m_id_planeHeight;
    int m_id_yTextureHeight;

    int m_id_plane1Texture;
    int m_id_plane2Texture;
    int m_id_colorMatrix;
    int m_id_opacity;

    int m_id_uvBlocksCount;
    int m_id_yBlocksPerLine;
};

} // namespace

class QnBpiSGVideoMaterial_YUV: public QSGMaterial
{
public:
    QnBpiSGVideoMaterial_YUV(const QVideoSurfaceFormat& format);
    ~QnBpiSGVideoMaterial_YUV();

    virtual QSGMaterialType* type() const
    {
        static QSGMaterialType biPlanarType;
        return &biPlanarType;
    }

    virtual QSGMaterialShader* createShader() const
    {
        return new QSGVideoMaterialShader_YUV_BiPlanarTiled();
    }

    virtual int compare(const QSGMaterial* other) const
    {
        const QnBpiSGVideoMaterial_YUV* m = static_cast<const QnBpiSGVideoMaterial_YUV*>(other);
        if (!m_textureIds[0])
            return 1;

        int d = m_textureIds[0] - m->m_textureIds[0];
        if (d)
            return d;
        else if ((d = m_textureIds[1] - m->m_textureIds[1]) != 0)
            return d;
        else
            return m_textureIds[2] - m->m_textureIds[2];
    }

    void updateBlending()
    {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void setCurrentFrame(const QVideoFrame& frame)
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = frame;
    }

    void bind();
    void bindTexture(int id, int w, int h, const uchar* bits, GLenum format);

    QVideoSurfaceFormat m_format;
    int m_textureSize;
    int m_planeCount;

    int m_uvBlocksCount;

    GLuint m_textureIds[3];
    GLfloat m_planeWidth;
    GLint m_framePixelWidth;
    GLint m_framePixelHeight;

    qreal m_opacity;
    QMatrix4x4 m_colorMatrix;

    QVideoFrame m_frame;
    QMutex m_frameMutex;
};

QnBpiSGVideoMaterial_YUV::QnBpiSGVideoMaterial_YUV(const QVideoSurfaceFormat& format):
    m_format(format),
    m_textureSize(0),
    m_opacity(1.0)
{
    memset(m_textureIds, 0, sizeof(m_textureIds));

    m_planeCount = 2;

    switch (format.yCbCrColorSpace())
    {
        case QVideoSurfaceFormat::YCbCr_JPEG:
            m_colorMatrix = QMatrix4x4(
                1.0f, 0.000f, 1.402f, -0.701f,
                1.0f, -0.344f, -0.714f, 0.529f,
                1.0f, 1.772f, 0.000f, -0.886f,
                0.0f, 0.000f, 0.000f, 1.0000f);
            break;

        case QVideoSurfaceFormat::YCbCr_BT709:
        case QVideoSurfaceFormat::YCbCr_xvYCC709:
            m_colorMatrix = QMatrix4x4(
                1.164f, 0.000f, 1.793f, -0.5727f,
                1.164f, -0.534f, -0.213f, 0.3007f,
                1.164f, 2.115f, 0.000f, -1.1302f,
                0.0f, 0.000f, 0.000f, 1.0000f);
            break;

        default: // BT 601
            m_colorMatrix = QMatrix4x4(
                1.164f, 0.000f, 1.596f, -0.8708f,
                1.164f, -0.392f, -0.813f, 0.5296f,
                1.164f, 2.017f, 0.000f, -1.081f,
                0.0f, 0.000f, 0.000f, 1.0000f);
    }

    setFlag(Blending, false);
}

QnBpiSGVideoMaterial_YUV::~QnBpiSGVideoMaterial_YUV()
{
    if (m_textureSize != 0)
    {
        if (QOpenGLContext* const current = QOpenGLContext::currentContext())
        {
            current->functions()->glDeleteTextures(m_planeCount, m_textureIds);
        }
        else
        {
            qWarning() << "QnBpiSGVideoMaterial_YUV:"
                <<"Cannot obtain GL context, unable to delete textures";
        }
    }
}

void QnBpiSGVideoMaterial_YUV::bind()
{
    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    QMutexLocker lock(&m_frameMutex);

    if (m_frame.isValid())
    {
        if (m_frame.map(QAbstractVideoBuffer::ReadOnly))
        {
            int fw = qPower2Ceil(m_frame.width(), 32);
            int fh = qPower2Ceil(m_frame.height(), 32);

            int fullSize = fw * fh;

            // Frame has changed its size, recreate textures.
            if (m_textureSize != fullSize)
            {
                if (m_textureSize != 0)
                    functions->glDeleteTextures(m_planeCount, m_textureIds);
                functions->glGenTextures(m_planeCount, m_textureIds);
                m_textureSize = fullSize;
            }

            GLint previousAlignment;
            functions->glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            m_framePixelWidth = m_frame.width();
            m_framePixelHeight = m_frame.height();

            {
                const int y = 0;
                const int uv = 1;

                m_planeWidth = qreal(fw) / m_frame.bytesPerLine(y);
                m_uvBlocksCount = m_textureSize / 2048;

                functions->glActiveTexture(GL_TEXTURE1);
                bindTexture(m_textureIds[1], 512, m_uvBlocksCount, m_frame.bits(uv),
                    GL_LUMINANCE_ALPHA);

                functions->glActiveTexture(GL_TEXTURE0); //< Finish with 0 as default texture unit.
                bindTexture(m_textureIds[0], 1024, m_uvBlocksCount, m_frame.bits(y),
                    GL_LUMINANCE_ALPHA);
            }

            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
            m_frame.unmap();
        }

        m_frame = QVideoFrame();
    }
    else
    {
        // Go backwards to finish with GL_TEXTURE0.
        for (int i = m_planeCount - 1; i >= 0; --i)
        {
            functions->glActiveTexture(GL_TEXTURE0 + i);
            functions->glBindTexture(GL_TEXTURE_2D, m_textureIds[i]);
        }
    }
}

void QnBpiSGVideoMaterial_YUV::bindTexture(int id, int w, int h, const uchar* bits, GLenum format)
{
    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();

    functions->glBindTexture(GL_TEXTURE_2D, id);
    functions->glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, bits);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

QnBpiSGVideoNode_YUV::QnBpiSGVideoNode_YUV(const QVideoSurfaceFormat& format):
    m_format(format)
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QnBpiSGVideoMaterial_YUV(format);
    setMaterial(m_material);
}

QnBpiSGVideoNode_YUV::~QnBpiSGVideoNode_YUV()
{
}

void QnBpiSGVideoNode_YUV::setCurrentFrame(const QVideoFrame& frame, FrameFlags)
{
    m_material->setCurrentFrame(frame);
    markDirty(DirtyMaterial);
}

void QSGVideoMaterialShader_YUV_BiPlanarTiled::updateState(
    const RenderState& state,
    QSGMaterial* newMaterial,
    QSGMaterial* /*oldMaterial*/)
{
    QnBpiSGVideoMaterial_YUV* mat = static_cast<QnBpiSGVideoMaterial_YUV*>(newMaterial);

    program()->setUniformValue(m_id_plane1Texture, 0);
    program()->setUniformValue(m_id_plane2Texture, 1);

    mat->bind();

    program()->setUniformValue(m_id_colorMatrix, mat->m_colorMatrix);
    program()->setUniformValue(m_id_planeWidth,
        (float) mat->m_planeWidth * mat->m_framePixelWidth);
    program()->setUniformValue(m_id_planeHeight, (float) mat->m_framePixelHeight);
    program()->setUniformValue(m_id_yTextureHeight, (GLint) (mat->m_framePixelHeight / 2.0));

    program()->setUniformValue(m_id_uvBlocksCount, (GLint) mat->m_uvBlocksCount);
    program()->setUniformValue(m_id_yBlocksPerLine, (GLint) (mat->m_framePixelWidth / 32));

    if (state.isOpacityDirty())
    {
        mat->m_opacity = state.opacity();
        program()->setUniformValue(m_id_opacity, GLfloat(mat->m_opacity));
    }
    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}
