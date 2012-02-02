#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QMutex>
#include <QWaitCondition>
#include <QtOpenGL>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include "render_status.h"

class CLVideoDecoderOutput;

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
    static int getMaxTextureSize();
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
    int checkOpenGLError() const;
    void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
    void updateTexture();
    void setForceSoftYUV(bool value);
    bool isYuvFormat() const;
    int glRGBFormat() const;

private:
#ifndef Q_OS_WIN
#   define APIENTRY
#endif
    typedef void (APIENTRY *_glProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
    typedef void (APIENTRY *_glBindProgramARB) (GLenum, GLuint);
    typedef void (APIENTRY *_glDeleteProgramsARB) (GLsizei, const GLuint *);
    typedef void (APIENTRY *_glGenProgramsARB) (GLsizei, GLuint *);
    typedef void (APIENTRY *_glProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    typedef void (APIENTRY *_glActiveTexture) (GLenum);
    _glProgramStringARB glProgramStringARB;
    _glBindProgramARB glBindProgramARB;
    _glDeleteProgramsARB glDeleteProgramsARB;
    _glGenProgramsARB glGenProgramsARB;
    _glProgramLocalParameter4fARB glProgramLocalParameter4fARB;
    _glActiveTexture glActiveTexture;

private:
    enum Program
    {
        YV12toRGB = 0,
        YUY2toRGB = 1,
        ProgramCount = 2
    };

    static QMutex m_programMutex;
    static bool m_programInited;
    static GLuint m_program[ProgramCount];
    static int gl_status;
    static QVector<uchar> m_staticYFiller;
    static QVector<uchar> m_staticUVFiller;
    static QList<GLuint *> m_garbage;

private:
    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;

    GLint m_clampConstant;
    bool m_isNonPower2;
    bool m_isSoftYuv2Rgb;

    GLuint m_texture[3];
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

    bool m_inited;
    int m_videoWidth;
    int m_videoHeight;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata;
};

#endif //QN_GL_RENDERER_H
