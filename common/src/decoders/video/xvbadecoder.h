////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef XVBADECODER_H
#define XVBADECODER_H

#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "amdxvba.h"
#include "abstractdecoder.h"
#include "../../utils/media/nalUnits.h"


namespace H264Profile
{
    static const int baseline = 66;
    static const int main = 77;
    static const int extended = 88;
    static const int high = 100;
    static const int high10 = 110;
    static const int high422 = 122;
    static const int high444 = 144;

    QString toString( unsigned int profile_idc );
}

namespace H264Level
{
    QString toString( const unsigned int level_idc, const int constraint_set3_flag = 0 );
}

//!base class for differently-stored pictures
class QnAbstractPictureData
{
public:
    enum PicStorageType
    {
        //!Picture data is stored in memory
        pstSysMemPic,
        //!Picture is stored as OpenGL texture
        pstOpenGL
    };

    //!Returns pic type
    virtual PicStorageType type() const = 0;
};

//!Picture data stored in system memory
class QnSysMemPictureData
:
    public QnAbstractPictureData
{
public:
    //TODO/IMPL

    virtual QnAbstractPictureData::PicStorageType type() const { return QnAbstractPictureData::pstSysMemPic; }
};

//!OpenGL texture (type \a pstOpenGL)
class QnOpenGLPictureData
:
    public QnAbstractPictureData
{
public:
    virtual QnAbstractPictureData::PicStorageType type() const { return QnAbstractPictureData::pstOpenGL; }

    //!Returns OGL texture name
    virtual unsigned int glTexture() const;
    //!Returns context of texture
    virtual GLXContext glContext() const;
};

//!Uses AMD XVBA API to decode h.264 stream
/*!
    To use it system MUST have Radeon videocard installed and AMD proprietary videocard driver.

    After object creation, one MUST check \a isHardwareAccelerationEnabled(). If it returns \a false, usage of the \a XVBADecoder object is prohibited (it will trigger an assert in decode)
*/
class QnXVBADecoder
:
    public QnAbstractVideoDecoder
{
public:
    /*!
        \param data acces unit of media stream. MUST contain sequence header (sps & pps in case of h.264)
    */
    QnXVBADecoder( const QnCompressedVideoDataPtr& data );
    virtual ~QnXVBADecoder();

    //!Implementation of AbstractDecoder::GetPixelFormat
    virtual PixelFormat GetPixelFormat();
    //!Implementation of AbstractDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
    //!Implementation of AbstractDecoder::resetDecoder
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    //!Implementation of AbstractDecoder::setOutPictureSize
    virtual void setOutPictureSize( const QSize& outSize );

private:
    typedef void* XVBASurface;

    class GLSurfaceContext
    {
    public:
        enum State
        {
            //!surface is ready to decode to
            ready,
            //!decoder still uses this surface
            decodingInProcess,
            //!contains decoded frame to be rendered to the screen
            readyToRender,
            //!surface is being used by the renderer
            renderingInProcess
        };

        unsigned int glTexture;
        XVBASurface surface;
        State state;
        //!Pts of a picture. Valid only in states \a decodingInProcess, \a readyToRender and \a renderingInProcess
        quint64 pts;

        GLSurfaceContext();
        GLSurfaceContext( unsigned int _glTexture, XVBASurface _surface );
    };

    //!Locks GL texture for rendering. While this object is alive, texture cannot be used for decoding
    class QnXVBAOpenGLPictureData
    :
        public QnOpenGLPictureData
    {
    public:
        QnXVBAOpenGLPictureData( GLSurfaceContext* ctx );
        virtual ~QnXVBAOpenGLPictureData();

        //!Returns OGL texture name
        virtual unsigned int glTexture() const;
        //!Returns context of texture
        virtual GLXContext glContext() const;

    private:
        GLSurfaceContext* m_ctx;
    };

    Status m_prevOperationStatus;
    void* m_context;
    void* m_decodeSession;
    GLXContext m_glContext;
    Display* m_display;
    unsigned int m_getCapDecodeOutputSize;
    std::vector<GLSurfaceContext> m_glSurfaces;
    XVBAPictureDescriptor m_pictureDescriptor;
    SPSUnit m_sps;
    PPSUnit m_pps;
    QSize m_outPicSize;

    XVBABufferDescriptor m_pictureDescriptorBuf;
    XVBABufferDescriptor* m_pictureDescriptorBufArray[1];
    XVBA_Decode_Picture_Input m_pictureDescriptorDecodeInput;

    XVBADecodeCap m_decodeCap;
    XVBABufferDescriptor m_ctrlBufDescriptor;
    XVBABufferDescriptor m_dataBufDescriptor;
    XVBA_Decode_Picture_Input m_srcDataDecodeInput;
    XVBABufferDescriptor* m_srcDataDecodeBufArray[2];
    std::vector<XVBADataCtrl> m_dataCtrlBuffers;

    XVBA_Decode_Picture_Start_Input m_decodeStartInput;
    XVBA_Decode_Picture_End_Input m_decodeEndInput;
    XVBA_Surface_Sync_Input m_syncIn;
    XVBA_Surface_Sync_Output m_syncOut;

    //!Creates XVBA context
    /*!
        \return true on success
    */
    bool createContext();
    //!Check if hardware decoder capable to decode media stream represented by packet \a data
    /*!
        \return true, if hardware accelerated decoding of is suppored, false otherises
    */
    bool checkDecodeCapabilities();
    //!Create XVBA decode session
    /*!
        \param data Access unit of media stream. MUST contain sequence header
        \return true on success
    */
    bool createDecodeSession( const QnCompressedVideoDataPtr& data );
    //!Creates OpenGL texture and XVBA surface and appends it to the \a m_glSurfaces array
    /*!
        \return true on success
    */
    bool createSurface();
    //!Parses sps & pps from \a data and fills \a m_pictureDescriptor
    bool readSequenceHeader( const QnCompressedVideoDataPtr& data );
    //!Finds all slices in \a data, adds element to \a dataCtrlBuffers for each slice, fills neccessary \a pictureDescriptor fields with values from the first slice
    void analyzeInputBufSlices(
        const QnCompressedVideoDataPtr& data,
        XVBAPictureDescriptor* const pictureDescriptor,
        std::vector<XVBADataCtrl>* const dataCtrlBuffers,
        size_t* firstSliceOffset );
    //!Checks all surfaces and updates state if neccessary
    /*!
        \param targetSurfaceCtx An empty surface is returned here. If there's no unused surface and
            total surface count is less than \a MAX_SURFACE_COUNT, calls \a createSurface to create new surface and returns the new one
        \param decodedPicSurface A decoded surface with lowest pts is returned here. NULL, if no decoded surfaces
    */
    void checkSurfaces( GLSurfaceContext** const targetSurfaceCtx, GLSurfaceContext** const decodedPicSurface );
    //!Guess what
    QString lastErrorText() const;
    QString decodeErrorToString( XVBA_DECODE_ERROR errorCode ) const;

    QnXVBADecoder( const QnXVBADecoder& );
    QnXVBADecoder& operator=( const QnXVBADecoder& );
};

#endif  //XVBADECODER_H
