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
#include "../ui/graphics/items/resource/decodedpicturetoopengluploader.h"


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

    /*!
        Called with corresponding QGLContext is surely alive
    */
    void beforeDestroy();
    
    Qn::RenderStatus paint(const QRectF &sourceRect, const QRectF &targetRect);

    bool isLowQualityImage() const;
    
    qint64 lastDisplayedTime() const;

    QnMetaDataV1Ptr lastFrameMetadata() const; 
    bool isHardwareDecoderUsed() const;

    bool isYV12ToRgbShaderUsed() const;
    bool isYV12ToRgbaShaderUsed() const;
    bool isNV12ToRgbShaderUsed() const;

    void setImageCorrectionEnabled(bool value) { m_imageCorrectionEnabled = value; }
    bool isImageCorrectionEnabled() const { return m_imageCorrectionEnabled; }
private:
    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation); // deprecated
private:
    const DecodedPictureToOpenGLUploader& m_decodedPictureProvider;
    qreal m_brightness;
    qreal m_contrast;
    qreal m_hue;
    qreal m_saturation;
    qint64 m_lastDisplayedTime;
    QnMetaDataV1Ptr m_lastDisplayedMetadata; // TODO: #Elric get rid of this
    unsigned m_lastDisplayedFlags;
    unsigned int m_prevFrameSequence;
    std::auto_ptr<QnYuy2ToRgbShaderProgram> m_yuy2ToRgbShaderProgram;
    std::auto_ptr<QnYv12ToRgbShaderProgram> m_yv12ToRgbShaderProgram;
    std::auto_ptr<QnYv12ToRgbaShaderProgram> m_yv12ToRgbaShaderProgram;
    std::auto_ptr<QnNv12ToRgbShaderProgram> m_nv12ToRgbShaderProgram;
    bool m_timeChangeEnabled;
    mutable QMutex m_mutex;
    bool m_imageCorrectionEnabled;

    void update( const QSharedPointer<CLVideoDecoderOutput>& curImg );
    //!Draws texture \a tex0ID to the screen
    void drawVideoTextureDirectly(
    	const QRectF& tex0Coords,
    	unsigned int tex0ID,
    	const float* v_array );
    //!Draws to the screen YV12 image represented with three textures (one for each plane YUV) using shader which mixes all three planes to RGB
    void drawYV12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    	const QRectF& tex0Coords,
    	unsigned int tex0ID,
    	unsigned int tex1ID,
    	unsigned int tex2ID,
    	const float* v_array );
    //!Draws YUV420 with alpha channel
    void drawYVA12VideoTexture(
        const DecodedPictureToOpenGLUploader::ScopedPictureLock& /*picLock*/,
	    const QRectF& tex0Coords,
	    unsigned int tex0ID,
	    unsigned int tex1ID,
	    unsigned int tex2ID,
	    unsigned int tex3ID,
	    const float* v_array );
    //!Draws to the screen NV12 image represented with two textures (Y-plane and UV-plane) using shader which mixes both planes to RGB
    void drawNV12VideoTexture(
    	const QRectF& tex0Coords,
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
};

#endif //QN_GL_RENDERER_H
