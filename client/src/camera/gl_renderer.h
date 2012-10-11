#ifndef QN_GL_RENDERER_H
#define QN_GL_RENDERER_H

#include <QtOpenGL>
#include <QScopedPointer>
#include <QSharedPointer>

#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */
#include <utils/media/frame_info.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/shaders/yuy2_to_rgb_shader_program.h>
#include <ui/graphics/shaders/yv12_to_rgb_shader_program.h>
#include <ui/graphics/shaders/nv12_to_rgb_shader_program.h>

#include "render_status.h"
#include "core/resource/resource_media_layout.h"


class CLVideoDecoderOutput;
class DecodedPictureToOpenGLUploader;

class QnGLRenderer
:
    public QnGlFunctions
{
public:
    static bool isPixelFormatSupported(PixelFormat pixfmt);

    QnGLRenderer( const QGLContext* context, const DecodedPictureToOpenGLUploader& decodedPictureProvider );
    ~QnGLRenderer();

    void beforeDestroy();
    //!Sets frame to draw to \a img and returns. Does not block till frame is actually rendered on screen
    void draw( const QSharedPointer<CLVideoDecoderOutput>& img );
    //!Blocks until current frame is rendererd
    void waitForFrameDisplayed(int channel);
    
    Qn::RenderStatus paint(const QRectF &r);

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);

    bool isLowQualityImage() const;
    qint64 lastDisplayedTime() const;
    QnMetaDataV1Ptr lastFrameMetadata() const; 

private:
    const QGLContext* m_context;
    const DecodedPictureToOpenGLUploader& m_decodedPictureProvider;
    mutable QMutex m_displaySync; // to avoid call paintEvent() more than once at the same time
    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata; // TODO: get rid of this
    unsigned m_lastDisplayedFlags;
    unsigned int m_prevFrameSequence;
    QScopedPointer<QnYuy2ToRgbShaderProgram> m_yuy2ToRgbShaderProgram;
    QScopedPointer<QnYv12ToRgbShaderProgram> m_yv12ToRgbShaderProgram;

    void update( const QSharedPointer<CLVideoDecoderOutput>& curImg );
    //!Draws texture \a tex0ID to the screen
    void drawVideoTextureDirectly(
    	const QVector2D& tex0Coords,
    	unsigned int tex0ID,
    	const float* v_array );
    //!Draws to the screen YV12 image represented with three textures (one for each plane YUV) using shader which mixes all three planes to RGB
    void drawYV12VideoTexture(
    	const QVector2D& tex0Coords,
    	unsigned int tex0ID,
    	unsigned int tex1ID,
    	unsigned int tex2ID,
    	const float* v_array );
    //!Draws to the screen NV12 image represented with two textures (Y-plane and UV-plane) using shader which mixes both planes to RGB
    void drawNV12VideoTexture(
    	const QVector2D& tex0Coords,
    	unsigned int tex0ID,
    	unsigned int tex1ID,
    	const float* v_array );
    //!Draws currently binded texturere
    /*!
     * 	\param v_array
     * 	\param tx_array texture vertexes array
     * */
    void drawBindedTexture( const float* v_array, const float* tx_array );
    void updateTexture( const QSharedPointer<CLVideoDecoderOutput>& curImg );
    bool isYuvFormat() const;
    int glRGBFormat() const;
    
    bool usingShaderYuvToRgb() const;

    static int maxTextureSize();
};

#endif //QN_GL_RENDERER_H
