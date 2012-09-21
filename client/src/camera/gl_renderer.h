#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QMutex>
#include <QMap>
#include <QWaitCondition>
#include <QtOpenGL>
#include <QScopedPointer>
#include <QSharedPointer>
#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */
#include "render_status.h"
#include "core/resource/resource_media_layout.h"

class CLVideoDecoderOutput;
class QnGLRendererPrivate;
class QnGlRendererTexture;

class QnGLRenderer
{
public:
    enum HardwareStatus {
        NOT_TESTED,
        SUPPORTED,
        NOT_SUPPORTED
    };

    QnGLRenderer(const QGLContext *context = NULL);
    ~QnGLRenderer();
    void beforeDestroy();

    static void clearGarbage();
    static bool isPixelFormatSupported(PixelFormat pixfmt);

    void draw(CLVideoDecoderOutput *image);
    void waitForFrameDisplayed(int channel);
    
    
    CLVideoDecoderOutput *update();
    Qn::RenderStatus paint(const QRectF &r);

    qreal opacity() const;
    void setOpacity(qreal opacity);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    bool isLowQualityImage() const;
    qint64 lastDisplayedTime() const;
    QnMetaDataV1Ptr lastFrameMetadata(int channel) const; 
    void setForceSoftYUV(bool value);

private:
    void drawVideoTexture(QnGlRendererTexture *tex0, QnGlRendererTexture *tex1, QnGlRendererTexture *tex2, const float *v_array);
    void updateTexture();
    bool isYuvFormat() const;
    int glRGBFormat() const;
    QnGlRendererTexture *texture(int index);
    
    bool usingShaderYuvToRgb() const;

    void ensureGlInitialized();

    static int maxTextureSize();

private:
    static QList<GLuint> m_garbage;

    friend class QnGlRendererTexture;

private:
    QSharedPointer<QnGLRendererPrivate> d;
    bool m_glInitialized;
    const QGLContext *m_context;

    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;

    enum {
        TEXTURE_COUNT = 3
    };

    QScopedPointer<QnGlRendererTexture> m_textures[TEXTURE_COUNT];

    bool m_forceSoftYUV;

    uchar* m_yuv2rgbBuffer;
    int m_yuv2rgbBufferLen;

    bool m_textureUploaded;

    int m_stride_old;
    int m_height_old;
    PixelFormat m_color_old;

    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qreal m_painterOpacity;

    bool m_needwait;
    bool m_newtexture;

    CLVideoDecoderOutput *m_curImg;
    
    int m_format;

    int m_videoWidth;
    int m_videoHeight;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata[CL_MAX_CHANNELS]; // TODO: get rid of this
    unsigned m_lastDisplayedFlags;
};

#endif //QN_GL_RENDERER_H
