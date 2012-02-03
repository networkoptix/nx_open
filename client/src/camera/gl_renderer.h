#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QMutex>
#include <QWaitCondition>
#include <QtOpenGL>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include "render_status.h"

class CLVideoDecoderOutput;
class QnGLRendererSharedData;


class QnGLRenderer : public QnRenderStatus
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
    RenderStatus paintEvent(const QRectF &r);

    qreal opacity() const;
    void setOpacity(qreal opacity);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    qint64 lastDisplayedTime() const;
    QnMetaDataV1Ptr lastFrameMetadata() const; 

private:
    void ensureInitialized();
    void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
    void updateTexture();
    void setForceSoftYUV(bool value);
    bool isYuvFormat() const;
    int glRGBFormat() const;

private:
    static QList<GLuint> m_garbage;

private:
    QnGLRendererSharedData *m_shared;

    bool m_initialized;

    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;

    GLint m_clampConstant;
    bool m_isNonPower2;
    bool m_isSoftYuv2Rgb;

    int m_textureStep;
    GLuint m_texture[6];
    bool m_forceSoftYUV;

    QVector<uchar> m_yuv2rgbBuffer;

    bool m_textureUploaded;

    int m_stride_old;
    int m_height_old;
    PixelFormat m_color_old;

    float m_videoCoeffL[4];
    float m_videoCoeffW[4];
    float m_videoCoeffH[4];

    bool m_videoTextureReady;

    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qreal m_painterOpacity;

    bool m_gotnewimage;
    bool m_needwait;

    CLVideoDecoderOutput *m_curImg;
    
    int m_format;

    int m_videoWidth;
    int m_videoHeight;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata;
};

#endif //QN_GL_RENDERER_H
