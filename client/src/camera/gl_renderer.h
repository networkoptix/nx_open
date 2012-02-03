#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QMutex>
#include <QWaitCondition>
#include <QtOpenGL>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include "render_status.h"

class CLVideoDecoderOutput;
class QnGLRendererSharedData;
class QnGlRendererTexture;

class QnGLRenderer: public QnRenderStatus
{
public:
    enum HardwareStatus {
        NOT_TESTED,
        SUPPORTED,
        NOT_SUPPORTED
    };

    QnGLRenderer();
    ~QnGLRenderer();
    void beforeDestroy();

    static void clearGarbage();
    static int maxTextureSize();
    static bool isPixelFormatSupported(PixelFormat pixfmt);

    void draw(CLVideoDecoderOutput *image);
    void waitForFrameDisplayed(int channel);
    
    
    void QnGLRenderer::update();
    RenderStatus paint(const QRectF &r);

    qreal opacity() const;
    void setOpacity(qreal opacity);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    qint64 lastDisplayedTime() const;
    QnMetaDataV1Ptr lastFrameMetadata() const; 

private:
    void drawVideoTexture(QnGlRendererTexture *tex0, QnGlRendererTexture *tex1, QnGlRendererTexture *tex2, const float *v_array);
    void updateTexture();
    void setForceSoftYUV(bool value);
    bool isYuvFormat() const;
    int glRGBFormat() const;
    QnGlRendererTexture *texture(int index);

private:
    static QList<GLuint> m_garbage;

    friend class QnGlRendererTexture;

private:
    QnGLRendererSharedData *m_shared;

    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;

    enum {
        TEXTURE_COUNT = 3
    };

    QScopedPointer<QnGlRendererTexture> m_textures[TEXTURE_COUNT];

    bool m_forceSoftYUV;

    uchar* m_yuv2rgbBuffer;
    size_t m_yuv2rgbBufferLen;

    bool m_textureUploaded;

    int m_stride_old;
    int m_height_old;
    PixelFormat m_color_old;

    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qreal m_painterOpacity;

    bool m_gotnewimage;
    bool m_needwait;
    bool m_newtexture;
    bool m_updated;

    CLVideoDecoderOutput *m_curImg;
    CLVideoDecoderOutput *m_textureImg;
    
    int m_format;

    int m_videoWidth;
    int m_videoHeight;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata;
};

#endif //QN_GL_RENDERER_H
