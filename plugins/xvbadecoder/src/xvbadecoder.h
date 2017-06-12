////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef XVBADECODER_H
#define XVBADECODER_H

#include <deque>
#include <fstream>
#include <string>

#include <QGLContext>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSize>
#include <QString>
#include <opengl/qgl_p.h>   //from Qt sources

#ifndef XVBA_TEST
#include <decoders/video/abstractdecoder.h>
#include <utils/media/nalUnits.h>
#else
#include "common.h"
#include "nalUnits.h"
#endif

#include <plugins/videodecoders/stree/resourcecontainer.h>

#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "amdxvba.h"


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

class QGLContextPrivateDuplicate
{
public:
};

//!Duplicates QGLContext to allow access to d_ptr (to access X11 context pointer finally)
class Q_OPENGL_EXPORT QGLContextDuplicate
{
    Q_DECLARE_PRIVATE(QGLContext)

public:
    virtual ~QGLContextDuplicate();

    enum BindOption
    {
        NoBindOption = 0x0000
    };
    Q_DECLARE_FLAGS(BindOptions, BindOption)

    QScopedPointer<QGLContextPrivateDuplicate> d_ptr;
};

class PluginUsageWatcher;

//!Uses AMD XVBA API to decode h.264 stream
/*!
    To use it system MUST have Radeon graphics card installed and AMD proprietary driver.

    After object creation, one MUST check \a isHardwareAccelerationEnabled(). If it returns \a false, usage of the \a XVBADecoder object is prohibited (it will trigger an assert in decode)
*/
class QnXVBADecoder
:
    public QnAbstractVideoDecoder,
    public nx::utils::stree::AbstractResourceReader
{
public:
    /*!
        \param data acces unit of media stream. MUST contain sequence header (sps & pps in case of h.264)
    */
    QnXVBADecoder(
        const QGLContext* const glContext,
        const QnCompressedVideoDataPtr& data,
        PluginUsageWatcher* const usageWatcher );
    virtual ~QnXVBADecoder();

    //!Implementation of AbstractDecoder::GetPixelFormat
    virtual PixelFormat GetPixelFormat() const;
    //!Implementation of AbstractDecoder::targetMemoryType
    virtual QnAbstractPictureData::PicStorageType targetMemoryType() const;
    //!Implementation of AbstractDecoder::getWidth
	virtual int getWidth() const;
    //!Implementation of AbstractDecoder::getHeight
	virtual int getHeight() const;
    //!Implementation of AbstractDecoder::getOriginalPictureSize
	virtual QSize getOriginalPictureSize() const;
    //!Implementation of AbstractDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
    //!Implementation of AbstractDecoder::resetDecoder
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    //!Implementation of AbstractDecoder::setOutPictureSize
    virtual void setOutPictureSize( const QSize& outSize );
#ifndef XVBA_TEST
    //!Implementation of AbstractDecoder::setLightCpuMode
    virtual void setLightCpuMode( QnAbstractVideoDecoder::DecodeMode );
#endif
    //!Implementation of nx::utils::stree::AbstractResourceReader::get
    /*!
        Following parameters are supported:\n
            - framePictureWidth
            - framePictureHeight
            - framePictureSize
            - fps
            - pixelsPerSecond
            - videoMemoryUsage
    */
    virtual bool get( int resID, QVariant* const value ) const;

private:
    typedef void* XVBASurface;

    class XVBASurfaceContext
    {
    public:
        enum State
        {
            //!surface is ready to decode to
            ready,
            //!decoder still uses this surface
            decodingInProgress,
            //!contains decoded frame to be rendered to the screen
            readyToRender,
            //!surface is being used by the renderer
            renderingInProgress
        };

        GLXContext glContext;
        unsigned int glTexture;
        XVBASurface surface;
        State state;
        //!Pts of a picture. Valid only in states \a decodingInProgress, \a readyToRender and \a renderingInProgress
        quint64 pts;
        std::vector<XVBABufferDescriptor*> usedDecodeBuffers;

        XVBASurfaceContext();
        XVBASurfaceContext(
        	GLXContext _glContext,
        	unsigned int _glTexture,
        	XVBASurface _surface );
        ~XVBASurfaceContext();

        //!Deallocates all surface resources
        void clear();

        static const char* toString( State );
    };

    //!Locks GL texture for rendering. While this object is alive, texture cannot be used for decoding
    class QnXVBAOpenGLPictureData
    :
        public QnOpenGLPictureData
    {
    public:
        QnXVBAOpenGLPictureData( const QSharedPointer<XVBASurfaceContext>& ctx );
        virtual ~QnXVBAOpenGLPictureData();

    private:
        /*!
            Holding shared pointer here, since XVBADecoder object can be deallocated earlier, than QnXVBAOpenGLPictureData
        */
        QSharedPointer<XVBASurfaceContext> m_ctx;
    };

    class SliceEx
    :
        public SliceUnit
    {
    public:
        int toppoc;
        int bottompoc;
        int PicOrderCntMsb;
        int ThisPOC;
        int framepoc;
        int AbsFrameNum;

        SliceEx();
    };

    class H264PPocCalcAuxiliaryData
    {
    public:
        int PrevPicOrderCntMsb;
        int PrevPicOrderCntLsb;
        int last_has_mmco_5;
        int last_pic_bottom_field;
        int ThisPOC;
        int PreviousFrameNum;
        int FrameNumOffset;
        int ExpectedDeltaPerPicOrderCntCycle;
        int PicOrderCntCycleCnt;
        int FrameNumInPicOrderCntCycle;
        int ExpectedPicOrderCnt;
        int PreviousFrameNumOffset;
        int MaxFrameNum;

        H264PPocCalcAuxiliaryData();
    };

    Status m_prevOperationStatus;
    void* m_context;
    void* m_decodeSession;
    GLXContext m_glContext;
    PluginUsageWatcher* const m_usageWatcher;
    Display* m_display;
    unsigned int m_getCapDecodeOutputSize;
    std::list<QSharedPointer<XVBASurfaceContext> > m_commonSurfaces;
    std::list<QSharedPointer<XVBASurfaceContext> > m_glSurfaces;
    XVBAPictureDescriptor m_pictureDescriptor;
    SPSUnit m_sps;
    PPSUnit m_pps;
    QSize m_outPicSize;

    XVBABufferDescriptor m_pictureDescriptorBuf;
    //!Holds XVBA_PICTURE_DESCRIPTION_BUFFER and XVBA_QM_BUFFER
    XVBABufferDescriptor* m_pictureDescriptorBufArray[2];
    XVBA_Decode_Picture_Input m_pictureDescriptorDecodeInput;

    XVBADecodeCap m_decodeCap;
//    XVBABufferDescriptor m_ctrlBufDescriptor;
//    XVBABufferDescriptor m_dataBufDescriptor;
    XVBA_Decode_Picture_Input m_srcDataDecodeInput;
    XVBABufferDescriptor* m_srcDataDecodeBufArray[2];

    XVBA_Decode_Picture_Start_Input m_decodeStartInput;
    XVBA_Decode_Picture_End_Input m_decodeEndInput;
    XVBA_Surface_Sync_Input m_syncIn;
    XVBA_Surface_Sync_Output m_syncOut;

    //!vector<pair<buffer, true if used> >
    std::vector<std::pair<XVBABufferDescriptor*, bool> > m_xvbaDecodeBuffers;

    quint8* m_decodedPictureRgbaBuf;
    size_t m_decodedPictureRgbaBufSize;
    //!number of common surfaces in state decodingInProgress
    size_t m_totalSurfacesBeingDecoded;

    std::ofstream m_inStream;

    H264PPocCalcAuxiliaryData m_h264PocData;

#ifdef XVBA_TEST
    bool m_hardwareAccelerationEnabled;
#endif
    //!See comment to \a DROP_SMALL_SECOND_SLICE macro
    bool m_checkForDroppableSecondSlice;
    int m_mbLinesToIgnore;
    //!Number of lines to crop at top of decoded picture
    unsigned int m_originalFrameCropTop;
    //!Number of lines to crop at bottom of decoded picture
    unsigned int m_originalFrameCropBottom;
    double m_sourceStreamFps;
    std::deque<qint64> m_fpsCalcFrameTimestamps;

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
    bool createDecodeSession();
    //!Creates OpenGL texture and XVBA surface and appends it to the \a m_glSurfaces array
    /*!
        \return true on success
    */
    bool createGLSurface();
    //!Creates common XVBA surface
    bool createSurface();
    //!Parses sps & pps from \a data and fills \a m_pictureDescriptor
    bool readSequenceHeader( const QnCompressedVideoDataPtr& data );
    //!Finds first slice in \a data, fills \a dataCtrlBuffer with data offset and size and fills neccessary \a pictureDescriptor fields with values from the first slice
    /*!
     * 	\return false if no slice found in \a data. true otherwise
     * */
    bool analyzeInputBufSlices(
        const QnCompressedVideoDataPtr& data,
        XVBAPictureDescriptor* const pictureDescriptor,
        std::vector<XVBABufferDescriptor*>* const dataCtrlBuffers );
    //!Checks all surfaces and updates state if neccessary
    /*!
        \param targetSurfaceCtx An empty surface is returned here. If there's no unused surface and
            total surface count is less than \a MAX_SURFACE_COUNT, calls \a createSurface to create new surface and returns the new one
        \param decodedPicSurface A decoded surface with lowest pts is returned here. NULL, if no decoded surfaces
    */
    void checkSurfaces( QSharedPointer<XVBASurfaceContext>* const targetSurfaceCtx, QSharedPointer<XVBASurfaceContext>* const decodedPicSurface );
    //!Guess what
    QString lastErrorText() const;
    QString decodeErrorToString( XVBA_DECODE_ERROR errorCode ) const;
    void fillOutputFrame( CLVideoDecoderOutput* const outFrame, QSharedPointer<XVBASurfaceContext> decodedPicSurface );
	XVBABufferDescriptor* getDecodeBuffer( XVBA_BUFFER bufferType );
	void ungetBuffer( XVBABufferDescriptor* bufDescriptor );
	void readSPS( const quint8* curNalu, const quint8* nextNalu );
	void readPPS( const quint8* curNalu, const quint8* nextNalu );
	XVBASurfaceContext* findUnusedGLSurface();
    //!Caculates h.264 picture order count
    void calcH264POC( SliceEx* const pSlice );
    void destroyXVBASession();
    Status XVBADecodePicture( XVBA_Decode_Picture_Input* decode_picture_input );
    /*!
        \param rightSlice Slice NALU starting with START_CODE_PREFIX
    */
    void appendSlice( XVBABufferDescriptor* leftSliceBuffer, const quint8* rightSlice, size_t rightSliceData );
    void dumpBuffer( const QString& msg, const quint8* buf, size_t bufSize );

	static int h264ProfileIdcToXVBAProfile( int profile_idc );

    QnXVBADecoder( const QnXVBADecoder& );
    QnXVBADecoder& operator=( const QnXVBADecoder& );
};

#endif  //XVBADECODER_H
