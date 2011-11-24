#ifndef clgl_renderer_12_29
#define clgl_renderer_12_29

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <QtOpenGL/qgl.h>

#include "abstractrenderer.h"

class CLVideoWindowItem;

class CLGLRenderer : public CLAbstractRenderer
{
public:
    enum CLGLDrawHardwareStatus
    {
        CL_GL_NOT_TESTED,
        CL_GL_SUPPORTED,
        CL_GL_NOT_SUPPORTED
    };

    static void clearGarbage();

    CLGLRenderer(CLVideoWindowItem *vw);
    CLGLRenderer();
    ~CLGLRenderer();

    static int getMaxTextureSize();

    virtual void draw(CLVideoDecoderOutput* image) override;
    virtual void waitForFrameDisplayed(int channel) override;

    bool paintEvent(const QRectF &r);

    virtual void beforeDestroy();

    QSize sizeOnScreen(unsigned int channel) const;
    bool constantDownscaleFactor() const;

    qreal opacity() const;
    void setOpacity(qreal opacity);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    static bool isPixelFormatSupported(PixelFormat pixfmt);

    QString getTimeText() const;
    bool isNoVideo() const;

    void onNoVideo();
private:
    void construct();
    void init(bool msgbox);
    static int gl_status;

private:
#ifndef Q_OS_WIN
    #define APIENTRY
#endif

    // ARB_fragment_program
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

    int checkOpenGLError() const;

    unsigned int getMinPow2(unsigned int value) const
    {
        unsigned int result = 1;
        while (value > result)
            result<<=1;

        return result;
    }

    void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
    void updateTexture();
    void setForceSoftYUV(bool value);
    bool isYuvFormat() const;
    int glRGBFormat() const;

private:
    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    QWaitCondition m_waitCon;

    GLint clampConstant;
    bool isNonPower2;
    bool isSoftYuv2Rgb;

    enum Program
    {
        YV12toRGB = 0,
        YUY2toRGB = 1,
        ProgramCount = 2
    };

    static QMutex m_programMutex;
    static bool m_programInited;
    static GLuint m_program[ProgramCount];
    GLuint m_texture[3];
    bool m_forceSoftYUV;

    QVector<uchar> yuv2rgbBuffer;

    bool m_textureUploaded;

    //int m_stride, // in memorry
    //    m_width, // visible width
    //    m_height,

    int m_stride_old;
    int m_height_old;

    //unsigned char *m_arrayPixels[3];

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

    CLVideoWindowItem *const m_videowindow;

    //CLVideoDecoderOutput m_image;
    //QQueue<CLVideoDecoderOutput*> m_imageList;

    CLVideoDecoderOutput* m_curImg;
    int m_format;
    //bool m_abort_drawing;

    //bool m_do_not_need_to_wait_any_more;

    bool m_inited;
    int m_videoWidth;
    int m_videoHeight;
    QString m_timeText;

    static QVector<uchar> m_staticYFiller;
    static QVector<uchar> m_staticUVFiller;
    bool m_noVideo;
};

#endif //clgl_renderer_12_29
