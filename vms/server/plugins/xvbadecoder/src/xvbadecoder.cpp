////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include <QDebug>

#include "xvbadecoder.h"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>

#include <QElapsedTimer>
#include <QX11Info>

#include <plugins/videodecoders/pluginusagewatcher.h>
#include <plugins/videodecoders/videodecoderplugintypes.h>

//decode to opengl texture
#define USE_OPENGL_SURFACE
//
#define USE_SOURCE_BUFFER
//#define SAVE_DECODED_FRAME_TO_FILE
//#define DUMP_DECODER_INPUT
#ifdef DUMP_DECODER_INPUT
//#define DUMP_BY_SLICES
#endif
//#define IGNORE_BUT_FIRST_SLICE
//#define IGNORE_BUT_LAST_SLICE
//#define JOIN_SLICES
//!Hack for Digital Watchdog camera. Decoder ignores second slice which 16 pix in height (one macroblock), since this slice is not properly decoded by XVBA
#define DROP_SMALL_SECOND_SLICE
#ifdef DROP_SMALL_SECOND_SLICE
static const int MAX_MACROBLOCK_LINES_TO_IGNORE = 1;
#endif

#if defined(JOIN_SLICES) && defined(USE_SOURCE_BUFFER)
#error "JOIN_SLICES and USE_SOURCE_BUFFER cannot be defined at the same time"
#endif

#if defined(JOIN_SLICES) && defined(IGNORE_BUT_FIRST_SLICE)
#error "JOIN_SLICES and IGNORE_BUT_FIRST_SLICE cannot be defined at the same time"
#endif

#if defined(JOIN_SLICES) && defined(IGNORE_BUT_LAST_SLICE)
#error "JOIN_SLICES and IGNORE_BUT_LAST_SLICE cannot be defined at the same time"
#endif


QGLContextDuplicate::~QGLContextDuplicate()
{
}

using namespace std;

/*
 * - унаследовать QnWorkbenchContextAware, чтобы иметь доступ к QnWorkbenchDisplay и из него получить текущий контекст (из QGLWidget'а)
 * - в QT opengl-классы не thread-safe и, возможно, придётся создавать контекст декодера в главном потоке приложения
 * - контекст декодера надо при создании шарить с главным контекстом приложения
 * */

static const unsigned int MAX_SURFACE_COUNT = 12;

namespace H264Profile
{
    QString toString( const unsigned int profile_idc )
    {
        switch( profile_idc )
        {
            case baseline:
                return QString::fromAscii("baseline");
            case main:
                return QString::fromAscii("main");
            case extended:
                return QString::fromAscii("extended");
            case high:
                return QString::fromAscii("high");
            case high10:
                return QString::fromAscii("high10");
            case high422:
                return QString::fromAscii("high422");
            case high444:
                return QString::fromAscii("high444");
            default:
                return QString::fromAscii("unknown");
        }
    }
}

namespace H264Level
{
    QString toString( const unsigned int level_idc, const int constraint_set3_flag )
    {
        return (level_idc == 11) && (constraint_set3_flag == 1)
                ? QString::fromAscii("1b")
                : QString::number(level_idc / 10.0, 'g', 1);
    }
}

template<class Numeric>
    Numeric ALIGN128( Numeric val )
{
    Numeric remainder = val & 0x7f;
    return remainder ? (val - remainder + 0x80) : val;
}


/////////////////////////////////////////////////////
//  class QnXVBADecoder::XVBASurfaceContext
/////////////////////////////////////////////////////
QnXVBADecoder::XVBASurfaceContext::XVBASurfaceContext()
:
    glTexture( 0 ),
    surface( NULL ),
    state( ready ),
    pts( 0 )
{
}

QnXVBADecoder::XVBASurfaceContext::XVBASurfaceContext(
	GLXContext _glContext,
	unsigned int _glTexture,
	XVBASurface _surface )
:
	glContext( _glContext ),
    glTexture( _glTexture ),
    surface( _surface ),
    state( ready ),
    pts( 0 )
{
}

QnXVBADecoder::XVBASurfaceContext::~XVBASurfaceContext()
{
    clear();
}

void QnXVBADecoder::XVBASurfaceContext::clear()
{
    if( surface )
    {
        XVBADestroySurface( surface );
        surface = NULL;
    }
    if( glTexture )
    {
        glDeleteTextures( 1, &glTexture );
        glTexture = 0;
    }
}

const char* QnXVBADecoder::XVBASurfaceContext::toString( State state )
{
	switch( state )
	{
		case ready:
			return "ready";
		case decodingInProgress:
			return "decodingInProgress";
		case readyToRender:
			return "readyToRender";
		case renderingInProgress:
			return "renderingInProgress";
		default:
			return "unknown";
	}
}


/////////////////////////////////////////////////////
//  class QnXVBADecoder::QnXVBAOpenGLPictureData
/////////////////////////////////////////////////////
QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData( const QSharedPointer<XVBASurfaceContext>& ctx )
:
    QnOpenGLPictureData(
//		ctx->glContext,
		ctx->glTexture ),
    m_ctx( ctx )
{
    NX_ASSERT(
    		ctx->state == XVBASurfaceContext::readyToRender,
    		"QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData",
    		QString::fromAscii("ctx->state = %1").arg(ctx->state).toAscii().data() );
    m_ctx->state = XVBASurfaceContext::renderingInProgress;
    cl_log.log( QString::fromAscii("Opengl surface moved to state %1").arg(QString::fromAscii(XVBASurfaceContext::toString(m_ctx->state))), cl_logDEBUG1 );
}

QnXVBADecoder::QnXVBAOpenGLPictureData::~QnXVBAOpenGLPictureData()
{
    m_ctx->state = XVBASurfaceContext::ready;
    cl_log.log( QString::fromAscii("Opengl surface moved to state %1").arg(QString::fromAscii(XVBASurfaceContext::toString(m_ctx->state))), cl_logDEBUG1 );
}


/////////////////////////////////////////////////////
//  class QnXVBADecoder::SliceEx
/////////////////////////////////////////////////////
QnXVBADecoder::SliceEx::SliceEx()
:
    toppoc( 0 ),
    bottompoc( 0 ),
    PicOrderCntMsb( 0 ),
    ThisPOC( 0 ),
    framepoc( 0 ),
    AbsFrameNum( 0 )
{
}

/////////////////////////////////////////////////////
//  class QnXVBADecoder::H264PPocCalcAuxiliaryData
/////////////////////////////////////////////////////
QnXVBADecoder::H264PPocCalcAuxiliaryData::H264PPocCalcAuxiliaryData()
:
    PrevPicOrderCntMsb( 0 ),
    PrevPicOrderCntLsb( 0 ),
    last_has_mmco_5( 0 ),
    last_pic_bottom_field( 0 ),
    ThisPOC( 0 ),
    PreviousFrameNum( 0 ),
    FrameNumOffset( 0 ),
    ExpectedDeltaPerPicOrderCntCycle( 0 ),
    PicOrderCntCycleCnt( 0 ),
    FrameNumInPicOrderCntCycle( 0 ),
    ExpectedPicOrderCnt( 0 ),
    PreviousFrameNumOffset( 0 ),
    MaxFrameNum( 0 )
{
}

/////////////////////////////////////////////////////
//  class QnXVBADecoder
/////////////////////////////////////////////////////
QnXVBADecoder::QnXVBADecoder(
    const QGLContext* const glContext,
    const QnCompressedVideoDataPtr& data,
    PluginUsageWatcher* const usageWatcher )
:
    m_prevOperationStatus( Success ),
    m_context( NULL ),
    m_decodeSession( NULL ),
    m_glContext( NULL ),
    m_usageWatcher( usageWatcher ),
    m_display( NULL ),
    m_getCapDecodeOutputSize( 0 ),
    m_decodedPictureRgbaBuf( NULL ),
    m_decodedPictureRgbaBufSize( 0 ),
    m_totalSurfacesBeingDecoded( 0 )
#ifdef XVBA_TEST
    ,m_hardwareAccelerationEnabled( false )
#endif
#ifdef DROP_SMALL_SECOND_SLICE
    ,m_checkForDroppableSecondSlice( true )
    ,m_mbLinesToIgnore(0)
#else
    ,m_checkForDroppableSecondSlice( false )
    ,m_mbLinesToIgnore(0)
#endif
    ,m_originalFrameCropTop(0)
    ,m_originalFrameCropBottom(0)
    ,m_sourceStreamFps( 0 )
{
    cl_log.log( QString::fromAscii("Initializing XVBA decoder"), cl_logDEBUG1 );

    m_display = QX11Info::display();    //retrieving display pointer
    m_hardwareAccelerationEnabled = true;

    if( !createContext() )
    {
        m_hardwareAccelerationEnabled = false;
        return;
    }

    if( !readSequenceHeader(data) || !checkDecodeCapabilities() )
    {
        m_hardwareAccelerationEnabled = false;
        return;
    }

    //creating OGL context shared with drawing context
    static int visualAttribList[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 4,
        GLX_GREEN_SIZE, 4,
        GLX_BLUE_SIZE, 4,
        None };
    XVisualInfo* visualInfo = glXChooseVisual( m_display, QX11Info::appScreen(), visualAttribList );
    m_glContext = glXCreateContext(
        m_display,
        visualInfo,
        glContext ? (GLXContext)(reinterpret_cast<const QGLContextPrivate*>(reinterpret_cast<const QGLContextDuplicate*>(glContext)->d_ptr.data())->cx) : NULL,
        true );
    if( !m_glContext )
    {
        XVBADestroyContext( m_context );
        m_context = NULL;
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create OpenGL context"), cl_logERROR );
    }

#ifdef DUMP_DECODER_INPUT
    m_inStream.open( "/home/ak/slices/total.264", std::ios_base::binary );
  	m_inStream.write( data->data.data(), data->data.size() );
#endif
}

QnXVBADecoder::~QnXVBADecoder()
{
    m_usageWatcher->decoderIsAboutToBeDestroyed( this );

    cl_log.log( QString::fromAscii("QnXVBADecoder. Destroying session..."), cl_logDEBUG1 );
    destroyXVBASession();

    if( m_context )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Destroying XVBA context..."), cl_logDEBUG1 );
        XVBADestroyContext( m_context );
        m_context = NULL;
    }

    if( m_glContext )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Destroying GLX context..."), cl_logDEBUG1 );
        glXDestroyContext( m_display, m_glContext );
    }

#ifdef SAVE_DECODED_FRAME_TO_FILE
    delete[] m_decodedPictureRgbaBuf;
    m_decodedPictureRgbaBuf = NULL;
#endif
}

//!Implementation of AbstractDecoder::GetPixelFormat
PixelFormat QnXVBADecoder::GetPixelFormat() const
{
#ifdef USE_OPENGL_SURFACE
    return AV_PIX_FMT_RGBA;
#else
    return AV_PIX_FMT_NV12;
#endif
}

QnAbstractPictureData::PicStorageType QnXVBADecoder::targetMemoryType() const
{
#ifdef USE_OPENGL_SURFACE
	return QnAbstractPictureData::pstOpenGL;
#else
	return QnAbstractPictureData::pstSysMemPic;
#endif
}

int QnXVBADecoder::getWidth() const
{
    return m_sps.getWidth();
}

int QnXVBADecoder::getHeight() const
{
    return m_sps.getHeight();
}

QSize QnXVBADecoder::getOriginalPictureSize() const
{
	return QSize( getWidth(), getHeight() );
}

//!Implementation of AbstractDecoder::decode
bool QnXVBADecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
//#ifdef DUMP_DECODER_INPUT
//  	m_inStream.write( data->data.data(), data->data.size() );
//#endif

    //NOTE decoder may use more than one surface at a time. We must not block until picture decoding is finished,
        //since decoder may require some more data to finish picture decoding
        //We can just remember, that this frame with pts is being decoded to this surface

    NX_ASSERT( m_hardwareAccelerationEnabled );
    NX_ASSERT( m_context );

    if( !m_decodeSession && !createDecodeSession() )
        return false;

    static const size_t FRAME_COUNT_TO_CALC_FPS = 20;
    static const size_t MIN_FRAME_COUNT_TO_CALC_FPS = FRAME_COUNT_TO_CALC_FPS / 2;

    //calculating m_sourceStreamFps
    const double fpsCalcTimestampDiff = m_fpsCalcFrameTimestamps.empty()
        ? 0
        : m_fpsCalcFrameTimestamps.back() - m_fpsCalcFrameTimestamps.front(); //TODO overflow?
    if( (m_fpsCalcFrameTimestamps.size() >= MIN_FRAME_COUNT_TO_CALC_FPS) && (fpsCalcTimestampDiff > 0) )
        m_sourceStreamFps = m_fpsCalcFrameTimestamps.size() / fpsCalcTimestampDiff;
    m_fpsCalcFrameTimestamps.push_back( data->timestamp );
    if( m_fpsCalcFrameTimestamps.size() > FRAME_COUNT_TO_CALC_FPS )
        m_fpsCalcFrameTimestamps.pop_front();

    XVBABufferDescriptor* pictureDescriptionBufferDescriptor = getDecodeBuffer( XVBA_PICTURE_DESCRIPTION_BUFFER );
    bool analyzeSrcDataResult = false;
    std::vector<XVBABufferDescriptor*> decodeCtrlBuffers;
    if( pictureDescriptionBufferDescriptor )
    {
        memcpy( pictureDescriptionBufferDescriptor->bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
        pictureDescriptionBufferDescriptor->data_size_in_buffer = sizeof(m_pictureDescriptor);
        m_pictureDescriptorBufArray[0] = pictureDescriptionBufferDescriptor;
        m_pictureDescriptorDecodeInput.num_of_buffers_in_list = 1;

        analyzeSrcDataResult = analyzeInputBufSlices(
            data,
            static_cast<XVBAPictureDescriptor*>(pictureDescriptionBufferDescriptor->bufferXVBA),
            &decodeCtrlBuffers );
    }

    //checking, if one of surfaces is done already
    QSharedPointer<XVBASurfaceContext> targetSurfaceCtx;
    QSharedPointer<XVBASurfaceContext> decodedPicSurface;
    checkSurfaces(
        analyzeSrcDataResult ? &targetSurfaceCtx : NULL,    //could not find any slice in input data ? no need to create surface
        &decodedPicSurface );
    NX_ASSERT( !decodedPicSurface || decodedPicSurface->state <= XVBASurfaceContext::renderingInProgress );

    if( decodedPicSurface )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Found decoded picture (pts %1). Providing it to output...").arg(decodedPicSurface->pts), cl_logDEBUG1 );
        //copying picture to output
    	fillOutputFrame( outFrame, decodedPicSurface );
        decodedPicSurface->state = XVBASurfaceContext::renderingInProgress;
    }

    if( !pictureDescriptionBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_PICTURE_DESCRIPTION_BUFFER buffer"), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    if( !analyzeSrcDataResult )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find slice in source data"), cl_logWARNING );
        std::for_each(
            decodeCtrlBuffers.begin(),
            decodeCtrlBuffers.end(),
            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        ungetBuffer( pictureDescriptionBufferDescriptor );
        return decodedPicSurface != NULL;
    }

    if( !targetSurfaceCtx )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find unused gl surface. Total surface number: %1").arg(m_commonSurfaces.size()), cl_logWARNING );
        return decodedPicSurface != NULL;
    }

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    //TODO/IMPL in case of interlaced video, possibly we'll have to split incoming frame to two fields

//    XVBABufferDescriptor* pictureDescriptionBufferDescriptor = getDecodeBuffer( XVBA_PICTURE_DESCRIPTION_BUFFER );
//    if( !pictureDescriptionBufferDescriptor )
//    {
//        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_PICTURE_DESCRIPTION_BUFFER buffer"), cl_logERROR );
//        return decodedPicSurface != NULL;
//    }
//	memcpy( pictureDescriptionBufferDescriptor->bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
//	pictureDescriptionBufferDescriptor->data_size_in_buffer = sizeof(m_pictureDescriptor);
    targetSurfaceCtx->usedDecodeBuffers.push_back( pictureDescriptionBufferDescriptor );
//    m_pictureDescriptorBufArray[0] = pictureDescriptionBufferDescriptor;
//    m_pictureDescriptorDecodeInput.num_of_buffers_in_list = 1;

	XVBABufferDescriptor* qmBufferDescriptor = getDecodeBuffer( XVBA_QM_BUFFER );
    if( !qmBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_QM_BUFFER buffer"), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.clear();
        return decodedPicSurface != NULL;
    }
    XVBAQuantMatrixAvc* quantMatrixAVC = static_cast<XVBAQuantMatrixAvc*>( qmBufferDescriptor->bufferXVBA );

    memset( quantMatrixAVC->bScalingLists4x4, 16, sizeof(quantMatrixAVC->bScalingLists4x4) );
    memset( quantMatrixAVC->bScalingLists8x8, 16, sizeof(quantMatrixAVC->bScalingLists8x8) );

    //TODO/IMPL check use_default flag
    unsigned char* quantMatrixStart = (unsigned char*)quantMatrixAVC->bScalingLists4x4;
    std::copy(
        (const int*)m_sps.ScalingList4x4,
        ((const int*)m_sps.ScalingList4x4) + sizeof(m_sps.ScalingList4x4)/sizeof(**m_sps.ScalingList4x4),
        quantMatrixStart );
    quantMatrixStart = (unsigned char*)quantMatrixAVC->bScalingLists8x8;
    std::copy(
        (const int*)m_sps.ScalingList8x8,
        ((const int*)m_sps.ScalingList8x8) + sizeof(m_sps.ScalingList8x8)/sizeof(**m_sps.ScalingList8x8),
        quantMatrixStart );

    qmBufferDescriptor->data_size_in_buffer = sizeof(*quantMatrixAVC);
    targetSurfaceCtx->usedDecodeBuffers.push_back( qmBufferDescriptor );
    m_pictureDescriptorBufArray[1] = qmBufferDescriptor;
    m_pictureDescriptorDecodeInput.num_of_buffers_in_list = 2;

#ifdef USE_SOURCE_BUFFER
    std::unique_ptr<XVBABufferDescriptor> dataBufferDescriptor( new XVBABufferDescriptor() );
    memset( dataBufferDescriptor.get(), 0, sizeof(*dataBufferDescriptor) );
    dataBufferDescriptor->size = sizeof(*dataBufferDescriptor);
    dataBufferDescriptor->bufferXVBA = const_cast<char*>(data->data.data());
    dataBufferDescriptor->buffer_size = data->data.size();
    dataBufferDescriptor->data_size_in_buffer = data->data.size();
    dataBufferDescriptor->buffer_type = XVBA_DATA_BUFFER;

    const size_t buffersBeforeDataBuffer = targetSurfaceCtx->usedDecodeBuffers.size() - 1;
#else
    XVBABufferDescriptor* dataBufferDescriptor = getDecodeBuffer( XVBA_DATA_BUFFER );
    if( !dataBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_BUFFER buffer"), cl_logERROR );
        std::for_each(
            targetSurfaceCtx->usedDecodeBuffers.begin(),
            targetSurfaceCtx->usedDecodeBuffers.end(),
            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.clear();
        return decodedPicSurface != NULL;
    }
    const size_t buffersBeforeDataBuffer = targetSurfaceCtx->usedDecodeBuffers.size();
    targetSurfaceCtx->usedDecodeBuffers.push_back( dataBufferDescriptor );

    memcpy( dataBufferDescriptor->bufferXVBA, data->data.data(), data->data.size() );
    dataBufferDescriptor->data_offset = 0;
    dataBufferDescriptor->data_size_in_buffer = data->data.size();
    memset( static_cast<quint8*>(dataBufferDescriptor->bufferXVBA) + dataBufferDescriptor->data_size_in_buffer, 0, 2 );
    dataBufferDescriptor->data_size_in_buffer += 2;
#endif

//    if( !analyzeInputBufSlices(
//    		data,
//    		static_cast<XVBAPictureDescriptor*>(pictureDescriptionBufferDescriptor->bufferXVBA),
//            &targetSurfaceCtx->usedDecodeBuffers ) )
//    {
//        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find slice in source data"), cl_logWARNING );
//        std::for_each(
//        	targetSurfaceCtx->usedDecodeBuffers.begin(),
//        	targetSurfaceCtx->usedDecodeBuffers.end(),
//        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
//        targetSurfaceCtx->usedDecodeBuffers.clear();
//        return decodedPicSurface != NULL;
//    }
    std::copy( decodeCtrlBuffers.begin(), decodeCtrlBuffers.end(), std::back_inserter(targetSurfaceCtx->usedDecodeBuffers) );

#if defined(IGNORE_BUT_FIRST_SLICE) || defined(IGNORE_BUT_LAST_SLICE)
    if( targetSurfaceCtx->usedDecodeBuffers.size() - (buffersBeforeDataBuffer+1) > 1 )
    {
#ifdef IGNORE_BUT_FIRST_SLICE
        //leaving only first slice of picture
        std::vector<XVBABufferDescriptor*>::iterator firstDataCtrlToEraseIter =
                targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1+1;
        std::for_each(
            firstDataCtrlToEraseIter,
            targetSurfaceCtx->usedDecodeBuffers.end(),
            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.erase( firstDataCtrlToEraseIter, targetSurfaceCtx->usedDecodeBuffers.end() );
#else	//IGNORE_BUT_LAST_SLICE
        //leaving only last slice of picture
        ungetBuffer( *(targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1) );
        targetSurfaceCtx->usedDecodeBuffers.erase( targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1 );
#endif
    }
#endif

    NX_ASSERT( targetSurfaceCtx->state == XVBASurfaceContext::ready );
    m_decodeStartInput.target_surface = targetSurfaceCtx->surface;
    m_prevOperationStatus = XVBAStartDecodePicture( &m_decodeStartInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not start picture decoding. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    //giving XVBA_PICTURE_DESCRIPTOR_BUFFER and XVBA_QM_BUFFER
    m_prevOperationStatus = ::XVBADecodePicture( &m_pictureDescriptorDecodeInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving XVBA_PICTURE_DESCRIPTOR_BUFFER and XVBA_QM_BUFFER to decoder. %1").arg(lastErrorText()), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.clear();
        destroyXVBASession();   //destroying session, since in case of such error usually we
        return decodedPicSurface != NULL;
    }

    //giving XVBA_DATA_BUFFER & XVBA_DATA_CTRL_BUFFER

    //align data buf to 128 bytes
    if( dataBufferDescriptor->buffer_size < data->data.size() )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Allocated XVBA_DATA_BUFFER size (%1) less than required (%2)").
        	arg(dataBufferDescriptor->buffer_size).arg(data->data.size()), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.clear();
        return decodedPicSurface != NULL;
    }

    //at this moment XVBADataCtrl buffers found in targetSurfaceCtx->usedDecodeBuffers point to slices in source buffer (data->data), not in XVBA_DATA_BUFFER

    //copying coded data to data buffer
    //char* dataStart = reinterpret_cast<char*>(dataBufferDescriptor->bufferXVBA);
    for( size_t i = buffersBeforeDataBuffer+1; i < targetSurfaceCtx->usedDecodeBuffers.size(); ++i )
    {
        XVBABufferDescriptor* inputBufArray[2];
#ifdef USE_SOURCE_BUFFER
        inputBufArray[0] = dataBufferDescriptor.get();
#else
        XVBADataCtrl* ctrl = ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA);
        ctrl->SliceBytesInBuffer += 2;
        ctrl->SliceBitsInBuffer += 2 << 3;	//adding two trailing zero bytes
        inputBufArray[0] = dataBufferDescriptor;
#endif
        inputBufArray[1] = targetSurfaceCtx->usedDecodeBuffers[i];

        m_srcDataDecodeInput.buffer_list = inputBufArray;
        m_srcDataDecodeInput.num_of_buffers_in_list = 2;
        m_prevOperationStatus = XVBADecodePicture( &m_srcDataDecodeInput );
        if( m_prevOperationStatus != Success )
        {
            cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving src data to decoder. %1. bufsize %2").
                arg(lastErrorText()).arg(dataBufferDescriptor->buffer_size, 0, 16), cl_logERROR );
            std::for_each(
                targetSurfaceCtx->usedDecodeBuffers.begin(),
                targetSurfaceCtx->usedDecodeBuffers.end(),
                std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
            targetSurfaceCtx->usedDecodeBuffers.clear();
            return decodedPicSurface != NULL;
        }
    }

    m_prevOperationStatus = XVBAEndDecodePicture( &m_decodeEndInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving Decode_Picture_End to decoder. %1").arg(lastErrorText()), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        targetSurfaceCtx->usedDecodeBuffers.clear();
        return decodedPicSurface != NULL;
    }

    targetSurfaceCtx->pts = data->timestamp;
    targetSurfaceCtx->state = XVBASurfaceContext::decodingInProgress;
    ++m_totalSurfacesBeingDecoded;

    cl_log.log( QString::fromAscii("QnXVBADecoder. Submitted %1 bytes of data (pts %2) to XVBA decoder (in %3 ms)").arg(data->data.size()).arg(data->timestamp).arg(elapsedTimer.elapsed()), cl_logDEBUG1 );
    return decodedPicSurface != NULL;
}

//!Implementation of AbstractDecoder::resetDecoder
void QnXVBADecoder::resetDecoder( QnCompressedVideoDataPtr /*data*/ )
{
    //TODO/IMPL
}

//!Implementation of AbstractDecoder::setOutPictureSize
void QnXVBADecoder::setOutPictureSize( const QSize& /*outSize*/ )
{
    //m_outPicSize = outSize;
}

#ifndef XVBA_TEST
void QnXVBADecoder::setLightCpuMode( QnAbstractVideoDecoder::DecodeMode )
{
    //TODO/IMPL
}
#endif

bool QnXVBADecoder::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::framePictureWidth:
            *value = m_sps.getWidth();
            return true;
        case DecoderParameter::framePictureHeight:
            *value = m_sps.getHeight();
            return true;
        case DecoderParameter::framePictureSize:
            *value = m_sps.getWidth() * m_sps.getHeight();
            return true;
        case DecoderParameter::fps:
            *value = m_sourceStreamFps;
            return true;
        case DecoderParameter::pixelsPerSecond:
            *value = m_sourceStreamFps * m_sps.getWidth() * m_sps.getHeight();
            return true;
        case DecoderParameter::videoMemoryUsage:
            *value = (qlonglong)m_glSurfaces.size() * m_sps.getWidth() * m_sps.getHeight() * 4; //texture uses 32-bit BGRA format
            return true;
        default:
            return false;
    }
}

bool QnXVBADecoder::createContext()
{
    {
        //===XVBACreateContext===
        XVBA_Create_Context_Input in;
        memset( &in, 0, sizeof(in) );
        in.size = sizeof(in);
        in.display = m_display;
        in.draw = QX11Info::appRootWindow(QX11Info::appScreen()); //get draw (main window?)

        XVBA_Create_Context_Output out;
        memset( &out, 0, sizeof(out) );
        out.size = sizeof(out);

        m_prevOperationStatus = XVBACreateContext( &in, &out );
        if( m_prevOperationStatus == Success )
        {
            m_context = out.context;
        }
        else
        {
            cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA context. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
            return false;
        }
    }

    {
        //===XVBAGetSessionInfo===
        XVBA_GetSessionInfo_Input in;
        memset( &in, 0, sizeof(in) );
        in.size = sizeof(in);

        XVBA_GetSessionInfo_Output out;
        memset( &out, 0, sizeof(out) );
        out.size = sizeof(out);

        m_prevOperationStatus = XVBAGetSessionInfo( &in, &out );
        if( m_prevOperationStatus != Success )
        {
            cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to get XVBA session info. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
            XVBADestroyContext( m_context );
            m_context = NULL;
            return false;
        }
        m_getCapDecodeOutputSize = out.getcapdecode_output_size;
    }

    return m_prevOperationStatus == Success;
}

bool QnXVBADecoder::checkDecodeCapabilities()
{
    //===XVBAGetCapDecode===
    XVBA_GetCapDecode_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    in.context = m_context;

    XVBA_GetCapDecode_Output out;
    memset( &out, 0, sizeof(out) );
    out.size = sizeof(out); //TODO/IMPL should use m_getCapDecodeOutputSize here
    m_prevOperationStatus = XVBAGetCapDecode( &in, &out );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to get XVBA decode capabilities. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

    bool accelerationSupported = false;
    unsigned int foundCapIndex = 0;
    for( foundCapIndex = 0; foundCapIndex < out.num_of_decodecaps; ++foundCapIndex )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Found capability: id %1, flags %2").arg(out.decode_caps_list[foundCapIndex].capability_id).
                arg(out.decode_caps_list[foundCapIndex].flags), cl_logDEBUG1 );

        if( out.decode_caps_list[foundCapIndex].capability_id != XVBA_H264 )
            continue;

//        if( (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_BASELINE && m_sps.profile_idc == H264Profile::baseline) ||
//            (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_MAIN && m_sps.profile_idc == H264Profile::main) ||
//            (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_HIGH && m_sps.profile_idc >= H264Profile::high) )
        {
            accelerationSupported = true;
            break;
        }
    }

    if( !accelerationSupported )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. h.264 (profile: %1, level: %2) decoding is not hardware accelerated on the host system").
            arg(H264Profile::toString(m_sps.profile_idc)).
            arg(H264Level::toString(m_sps.level_idc)),
            cl_logERROR );
        return false;
    }

    memcpy( &m_decodeCap, &out.decode_caps_list[foundCapIndex], sizeof(m_decodeCap) );

    cl_log.log( QString::fromAscii("QnXVBADecoder. h.264 (profile: %1, level: %2) decoding is hardware accelerated on the host system").
        arg(H264Profile::toString(m_sps.profile_idc)).
        arg(H264Level::toString(m_sps.level_idc)),
        cl_logDEBUG1 );

    return true;
}

bool QnXVBADecoder::createDecodeSession()
{
    //===XVBACreateDecode===
    XVBA_Create_Decode_Session_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    if( m_outPicSize.isEmpty() )
    {
        in.width = m_sps.pic_width_in_mbs*16;
        in.height = m_sps.getHeight();
        cl_log.log( QString::fromAscii("Creating XVBA decode session for pic %1x%2").arg(in.width).arg(in.height), cl_logDEBUG1 );
    }
    else
    {
        in.width = m_outPicSize.width();
        in.height = m_outPicSize.height();
    }
    in.context = m_context;

    m_decodeCap.size            = sizeof m_decodeCap;
    m_decodeCap.capability_id   = XVBA_H264;
    m_decodeCap.flags           = XVBA_H264_HIGH;   //TODO
    m_decodeCap.surface_type    = XVBA_NV12;

    in.decode_cap = &m_decodeCap;

    XVBA_Create_Decode_Session_Output out;
    memset( &out, 0, sizeof(out) );
    out.size = sizeof(out);

    m_prevOperationStatus = XVBACreateDecode( &in, &out );
    if( m_prevOperationStatus == Success )
    {
        m_decodeSession = out.session;
    }
    else
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA decode session. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

    cl_log.log( QString::fromAscii("QnXVBADecoder. Created XVBA decode session"), cl_logINFO );

    //preparing decode buffers
        //preparing picture_decode_start
    memset( &m_decodeStartInput, 0, sizeof(m_decodeStartInput) );
    m_decodeStartInput.size = sizeof(m_decodeStartInput);
    m_decodeStartInput.session = m_decodeSession;

        //preparing buffer with picture info
    memset( &m_pictureDescriptorBuf, 0, sizeof(m_pictureDescriptorBuf) );
    m_pictureDescriptorBuf.size = sizeof(m_pictureDescriptorBuf);
    m_pictureDescriptorBuf.buffer_type = XVBA_PICTURE_DESCRIPTION_BUFFER;
    m_pictureDescriptorBuf.buffer_size = sizeof(m_pictureDescriptor);
    m_pictureDescriptorBuf.bufferXVBA = &m_pictureDescriptor;
    m_pictureDescriptorBuf.data_size_in_buffer = sizeof(m_pictureDescriptor);

    memset( &m_pictureDescriptorDecodeInput, 0, sizeof(m_pictureDescriptorDecodeInput) );
    m_pictureDescriptorDecodeInput.size = sizeof(m_pictureDescriptorDecodeInput);
    m_pictureDescriptorDecodeInput.session = m_decodeSession;
    m_pictureDescriptorBufArray[0] = &m_pictureDescriptorBuf;
    m_pictureDescriptorDecodeInput.num_of_buffers_in_list = sizeof(m_pictureDescriptorBufArray) / sizeof(*m_pictureDescriptorBufArray);
    m_pictureDescriptorDecodeInput.buffer_list = m_pictureDescriptorBufArray;

        //preparing buffer with stream data
//    memset( &m_ctrlBufDescriptor, 0, sizeof(m_ctrlBufDescriptor) );
//    m_ctrlBufDescriptor.size = sizeof(m_ctrlBufDescriptor);
//    m_ctrlBufDescriptor.buffer_type = XVBA_DATA_CTRL_BUFFER;
//    memset( &m_dataBufDescriptor, 0, sizeof(m_dataBufDescriptor) );
//    m_dataBufDescriptor.size = sizeof(m_dataBufDescriptor);
//    m_dataBufDescriptor.buffer_type = XVBA_DATA_BUFFER;

//    m_srcDataDecodeBufArray[0] = &m_dataBufDescriptor;
//    m_srcDataDecodeBufArray[1] = &m_ctrlBufDescriptor;

    memset( &m_srcDataDecodeInput, 0, sizeof(m_srcDataDecodeInput) );
    m_srcDataDecodeInput.size = sizeof( m_srcDataDecodeInput );
    m_srcDataDecodeInput.session = m_decodeSession;
    m_srcDataDecodeInput.num_of_buffers_in_list = 2;
    m_srcDataDecodeInput.buffer_list = m_srcDataDecodeBufArray;

        //preparing picture_decode_end
    memset( &m_decodeEndInput, 0, sizeof(m_decodeEndInput) );
    m_decodeEndInput.size = sizeof(m_decodeEndInput);
    m_decodeEndInput.session = m_decodeSession;

    memset( &m_syncIn, 0, sizeof(m_syncIn) );
    m_syncIn.size = sizeof(m_syncIn);
    m_syncIn.session = m_decodeSession;

#ifdef SAVE_DECODED_FRAME_TO_FILE
    m_decodedPictureRgbaBufSize = in.width * in.height * 4;
    m_decodedPictureRgbaBuf = new quint8[m_decodedPictureRgbaBufSize];
    memset( m_decodedPictureRgbaBuf, 0xf0, m_decodedPictureRgbaBufSize );
    for( size_t i = 0; i < m_decodedPictureRgbaBufSize; i+=2 )
    {
    	m_decodedPictureRgbaBuf[i] = 0xf8;
    	m_decodedPictureRgbaBuf[i+1] = 0x30;
    }
#endif

    if( m_prevOperationStatus != Success )
        return false;

    m_usageWatcher->decoderCreated( this );
    return true;
}

bool QnXVBADecoder::createGLSurface()
{
    //creating gl texture
    if( !glXMakeCurrent(
            m_display,
            QX11Info::appRootWindow(QX11Info::appScreen()),
            m_glContext ) )
    {
        GLenum errorCode = glGetError();
        m_prevOperationStatus = BadDrawable;
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to make opengl context current. %u").arg(errorCode), cl_logERROR );
        return false;
    }

    GLuint gltexture = 0;
    glGenTextures( 1, &gltexture );
    GLenum glErrorCode = 0;
    if( gltexture == 0 )
    {
        glErrorCode = glGetError();
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create gltexture. %1").arg(glErrorCode), cl_logERROR );

        if( !glXMakeCurrent(
                m_display,
                None,
                NULL ) )
        {
            GLenum errorCode = glGetError();
            m_prevOperationStatus = BadDrawable;
            cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to reset current opengl context. %u").arg(errorCode), cl_logERROR );
            return false;
        }
        return false;
    }
    glBindTexture( GL_TEXTURE_2D, gltexture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
    glTexImage2D( GL_TEXTURE_2D, 0,
    			  GL_RGBA,
    			  m_sps.pic_width_in_mbs*16,
                  m_sps.getHeight(),
                  0,
                  GL_BGRA,
                  GL_UNSIGNED_BYTE,  //GL_UNSIGNED_SHORT_4_4_4_4,
                  NULL );
//    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
//    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glErrorCode = glGetError();
    if( glErrorCode )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create gltexture with size %1x%2. %3").
       		arg(m_sps.pic_width_in_mbs*16).arg(m_sps.getHeight()).arg(glErrorCode), cl_logERROR );
    }
    cl_log.log( QString::fromAscii("Created opengl texture %1x%2").arg(m_sps.pic_width_in_mbs*16).arg(m_sps.getHeight()), cl_logDEBUG1 );

    XVBA_Create_GLShared_Surface_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    in.session = m_decodeSession;
    in.glcontext = m_glContext;
    in.gltexture = gltexture;

    XVBA_Create_GLShared_Surface_Output out;
    memset( &out, 0, sizeof(out) );
    out.size = sizeof(out);
    m_prevOperationStatus = XVBACreateGLSharedSurface( &in, &out );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA OpenGL surface. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        glDeleteTextures( 1, &gltexture );

        if( !glXMakeCurrent(
                m_display,
                None,
                NULL ) )
        {
            GLenum errorCode = glGetError();
            m_prevOperationStatus = BadDrawable;
            cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to reset current opengl context. %u").arg(errorCode), cl_logERROR );
            return false;
        }
        return false;
    }

    m_glSurfaces.push_back( QSharedPointer<XVBASurfaceContext>( new XVBASurfaceContext( m_glContext, in.gltexture, out.surface ) ) );
    cl_log.log( QString::fromAscii("XVBA opengl surface created successfully. Total surface count: %1" ).arg(m_commonSurfaces.size()), cl_logDEBUG1 );

    if( !glXMakeCurrent(
            m_display,
            None,
            NULL ) )
    {
        GLenum errorCode = glGetError();
        m_prevOperationStatus = BadDrawable;
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to reset current opengl context. %u").arg(errorCode), cl_logERROR );
        return false;
    }

    return true;
}

bool QnXVBADecoder::createSurface()
{
    XVBA_Create_Surface_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    in.session = m_decodeSession;
    in.width = m_pictureDescriptor.width_in_mb * 16;
    in.height = m_sps.getHeight();
    in.surface_type = XVBA_NV12;

    XVBA_Create_Surface_Output out;
    memset( &out, 0, sizeof(out) );
    out.size = sizeof(out);

    m_prevOperationStatus = XVBACreateSurface( &in, &out );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA surface. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

    m_commonSurfaces.push_back( QSharedPointer<XVBASurfaceContext>( new XVBASurfaceContext( NULL, 0, out.surface ) ) );
    cl_log.log( QString::fromAscii("XVBA surface (%1x%2) created successfully. Total surface count: %3" ).
                arg(in.width).arg(in.height).
                arg(m_commonSurfaces.size()), cl_logDEBUG1 );
    return true;
}

static const int H264_START_CODE_PREFIX_LENGTH = 3;

bool QnXVBADecoder::readSequenceHeader( const QnCompressedVideoDataPtr& data )
{
	if( data->data.size() <= 4 )
        return false;

    memset( &m_pictureDescriptor, 0, sizeof(m_pictureDescriptor) );

    bool spsFound = false;
    bool ppsFound = false;
    const quint8* dataStart = reinterpret_cast<const quint8*>(data->data.data());
    const quint8* dataEnd = dataStart + data->data.size();
    for( const quint8
         *curNalu = NALUnit::findNextNAL( dataStart, dataEnd ),
         *nextNalu = NULL;
         curNalu < dataEnd;
         curNalu = nextNalu )
    {
        nextNalu = NALUnit::findNextNAL( curNalu, dataEnd );
        NX_ASSERT( nextNalu > curNalu );
        const quint8* curNaluEnd = nextNalu == dataEnd ? dataEnd : (nextNalu - H264_START_CODE_PREFIX_LENGTH);
        //skipping trailing_zero_bits
        for( --curNaluEnd; (curNaluEnd > curNalu) && (*curNaluEnd == 0); --curNaluEnd ) {}
        if( curNaluEnd != dataEnd )
            ++curNaluEnd;
        switch( *curNalu & 0x1f )
        {
            case nuSPS:
            {
                readSPS( curNalu, curNaluEnd );
            	spsFound = true;
                break;
            }

            case nuPPS:
            {
                readPPS( curNalu, curNaluEnd );
            	ppsFound = true;
                break;
            }

            default:
                break;
        }
    }

    return spsFound && ppsFound;
}

bool QnXVBADecoder::analyzeInputBufSlices(
    const QnCompressedVideoDataPtr& data,
    XVBAPictureDescriptor* const pictureDescriptor,
    std::vector<XVBABufferDescriptor*>* const dataCtrlBuffers )
{
    const size_t sizeBak = dataCtrlBuffers->size();

#ifdef DROP_SMALL_SECOND_SLICE
    bool mayEstablishSecondSliceDropping = false;
#endif
    int analyzedSliceCount = 0;
    //searching for a first slice
    const quint8* dataEnd = reinterpret_cast<const quint8*>(data->data.data()) + data->data.size();
    for( const quint8
         *curNalu = NALUnit::findNextNAL( reinterpret_cast<const quint8*>(data->data.data()), dataEnd ),
         *nextNalu = NULL;
         curNalu < dataEnd;
         curNalu = nextNalu )
    {
    	const quint8* curNaluWithPrefix = curNalu - H264_START_CODE_PREFIX_LENGTH;
        nextNalu = NALUnit::findNextNAL( curNalu, dataEnd );
        NX_ASSERT( nextNalu > curNalu );
        //const quint8* curNaluEnd = nextNalu == dataEnd ? dataEnd : (nextNalu - H264_START_CODE_PREFIX_LENGTH);
        const quint8* curNaluEnd = nextNalu == dataEnd ? dataEnd : (nextNalu - H264_START_CODE_PREFIX_LENGTH);
        //skipping trailing_zero_bits
        for( --curNaluEnd; (curNaluEnd > curNalu) && (*curNaluEnd == 0); --curNaluEnd ) {}
        if( curNaluEnd != dataEnd )
        	++curNaluEnd;
        //TODO/IMPL calculate exact NALU end, since there could be zero bytes after NALU
        switch( *curNalu & 0x1f )
        {
			case nuSPS:
                readSPS( curNalu, curNaluEnd );
#ifdef DROP_SMALL_SECOND_SLICE
                if( mayEstablishSecondSliceDropping )
                {
                    //checking here to avoid double decrement (in readSPS() and at the end of this method)
                    pictureDescriptor->height_in_mb = m_pictureDescriptor.height_in_mb;
                    mayEstablishSecondSliceDropping = false;
                }
#endif
                break;

			case nuPPS:
                readPPS( curNalu, curNaluEnd );
				break;

			case nuSliceNonIDR:
            case nuSliceA:
            case nuSliceIDR:
            {
                SliceEx slice;
                slice.decodeBuffer( curNalu, curNaluEnd );
                slice.deserialize( &m_sps, &m_pps );
                calcH264POC( &slice );
                if( slice.slice_type == SliceUnit::B_TYPE )
                {
                    ++analyzedSliceCount;
                    break;
                }

                //parsing slice_header
                pictureDescriptor->picture_structure = pictureDescriptor->sps_info.avc.frame_mbs_only_flag || (slice.m_field_pic_flag == 0)
                    ? 3  //3 = Frame
                    : slice.bottom_field_flag;  //1 = Bottom field, 0 = Top field
//                pictureDescriptor->picture_structure = 3;

                pictureDescriptor->avc_frame_num = slice.frame_num;
                //std::cout<<"Found slice pic_struct = "<<pictureDescriptor->picture_structure<<", slice.frame_num = "<<slice.frame_num<<"\n";
                //TODO following values are only suitable in progressive baseline video
                pictureDescriptor->avc_curr_field_order_cnt_list[0] = slice.toppoc;    //TopFieldOrderCnt
                pictureDescriptor->avc_curr_field_order_cnt_list[1] = slice.bottompoc;    //BottomFieldOrderCnt
//                pictureDescriptor->avc_curr_field_order_cnt_list[0] = slice.frame_num * 2;    //TopFieldOrderCnt
//                pictureDescriptor->avc_curr_field_order_cnt_list[1] = slice.frame_num * 2;    //BottomFieldOrderCnt
                pictureDescriptor->avc_intra_flag = slice.slice_type == SliceUnit::I_TYPE ? 1 : 0;    //I - frame. Supposing, that only I frames are intra coded
                pictureDescriptor->avc_reference = slice.nal_ref_idc > 0 ? 1 : 0;

#ifdef DROP_SMALL_SECOND_SLICE
                if( m_checkForDroppableSecondSlice && (analyzedSliceCount == 1) )
                {
//                    cl_log.log( QString::fromAscii("QnXVBADecoder. Checking slice for drop. first_mb_in_slice %1, pic_width_in_mbs %2, pic_height_in_map_units %3").
//                                arg(slice.first_mb_in_slice).arg(m_sps.pic_width_in_mbs).arg(m_sps.pic_height_in_map_units), cl_logDEBUG1 );
                    //assuming source buf contains full AU
                    if( (slice.first_mb_in_slice % m_sps.pic_width_in_mbs == 0) &&  //slice contains integer number of lines
                        (m_sps.pic_height_in_map_units - slice.first_mb_in_slice / m_sps.pic_width_in_mbs <= MAX_MACROBLOCK_LINES_TO_IGNORE) )
                    {
                        mayEstablishSecondSliceDropping = true;
                        m_checkForDroppableSecondSlice = false;
                        m_mbLinesToIgnore = m_sps.pic_height_in_map_units - slice.first_mb_in_slice / m_sps.pic_width_in_mbs;
                        cl_log.log( QString::fromAscii("QnXVBADecoder. Establishing dropping of %1 bottom mb line(s)").arg(m_mbLinesToIgnore), cl_logDEBUG1 );
                    }
                }
#endif
            }

			case nuSliceB:
            case nuSliceC:
            case nuSliceWithoutPartitioning:
            {
                ++analyzedSliceCount;
                XVBABufferDescriptor* dataCtrlBufferDescriptor = getDecodeBuffer( XVBA_DATA_CTRL_BUFFER );
                if( !dataCtrlBufferDescriptor )
                {
                    cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_CTRL_BUFFER buffer"), cl_logERROR );
                	return false;
                }
                XVBADataCtrl* dataCtrlBuffer = static_cast<XVBADataCtrl*>(dataCtrlBufferDescriptor->bufferXVBA);
                memset( dataCtrlBuffer, 0, sizeof(*dataCtrlBuffer) );
                dataCtrlBuffer->SliceDataLocation = curNaluWithPrefix - reinterpret_cast<const quint8*>(data->data.data());    //TODO offset to slice() or to slice_data()?
                dataCtrlBuffer->SliceBytesInBuffer = (curNaluEnd - curNaluWithPrefix);
                dataCtrlBuffer->SliceBitsInBuffer = dataCtrlBuffer->SliceBytesInBuffer << 3;

                cl_log.log( QString::fromAscii( "Found NAL (%1) at %2, %3 bytes long. Total buffer %4 bytes" ).
                		arg(*curNalu & 0x1f).arg(dataCtrlBuffer->SliceDataLocation).arg(dataCtrlBuffer->SliceBytesInBuffer).arg(data->data.size()), cl_logDEBUG1 );

                dataCtrlBuffers->push_back( dataCtrlBufferDescriptor );
                break;
            }

            default:
                continue;
        }
    }

#ifdef DROP_SMALL_SECOND_SLICE
    if( mayEstablishSecondSliceDropping )
    {
        m_sps.pic_height_in_map_units -= m_mbLinesToIgnore;
        pictureDescriptor->height_in_mb -= m_mbLinesToIgnore;
        m_pictureDescriptor.height_in_mb -= m_mbLinesToIgnore;
    }
#endif

    return dataCtrlBuffers->size() > sizeBak;
}

void QnXVBADecoder::checkSurfaces( QSharedPointer<XVBASurfaceContext>* const targetSurfaceCtx, QSharedPointer<XVBASurfaceContext>* const decodedPicSurface )
{
    for( std::list<QSharedPointer<XVBASurfaceContext> >::iterator
         it = m_commonSurfaces.begin();
         it != m_commonSurfaces.end();
          )
    {
        switch( (*it)->state )
        {
            case XVBASurfaceContext::ready:
                if( targetSurfaceCtx && !*targetSurfaceCtx )
                    *targetSurfaceCtx = *it;
                break;

            case XVBASurfaceContext::decodingInProgress:
                m_syncIn.surface = it->data()->surface;
                m_syncIn.query_status = (XVBA_QUERY_STATUS)(XVBA_GET_SURFACE_STATUS);	//XVBA_GET_DECODE_ERRORS not supported
                memset( &m_syncOut, 0, sizeof(m_syncOut) );
                m_syncOut.size = sizeof(m_syncOut);
                //m_syncOut.decode_error.size = sizeof(m_syncOut.decode_error.size);
                m_prevOperationStatus = XVBASyncSurface( &m_syncIn, &m_syncOut );
                if( m_prevOperationStatus != Success )
                {
                    cl_log.log( QString::fromAscii("QnXVBADecoder. Error synchronizing decoding. %1").arg(lastErrorText()), cl_logERROR );
                    break;
                }
                if( m_syncOut.status_flags & XVBA_STILL_PENDING )
                    break;   //surface still being used
                cl_log.log( QString::fromAscii("QnXVBADecoder. Surface sync result: %1").arg(m_syncOut.status_flags), cl_logDEBUG2 );

                if( m_syncOut.status_flags & XVBA_COMPLETED )
                {
#ifdef USE_OPENGL_SURFACE
                	//searching for an unused GL surface
                	XVBASurfaceContext* unusedGLSurface = findUnusedGLSurface();

                	//moving decoded data to opengl surface
                	XVBA_Transfer_Surface_Input transIn;
                	memset( &transIn, 0, sizeof(transIn) );
                	transIn.size = sizeof( transIn );
                	transIn.session = m_decodeSession;
                	transIn.src_surface = it->data()->surface;
                	transIn.target_surface = unusedGLSurface->surface;
                	transIn.flag = XVBA_FRAME;
                	m_prevOperationStatus = XVBATransferSurface( &transIn );
                    if( m_prevOperationStatus != Success )
                    {
                        cl_log.log( QString::fromAscii("QnXVBADecoder. Error transferring surface to GL. %1").arg(lastErrorText()), cl_logERROR );
                        //assuming surface still being used
                        break;
                    }

                    unusedGLSurface->state = XVBASurfaceContext::readyToRender;
                    unusedGLSurface->pts = (*it)->pts;
                    (*it)->state = XVBASurfaceContext::ready;
#else
                    (*it)->state = XVBASurfaceContext::readyToRender;
#endif
                    --m_totalSurfacesBeingDecoded;

                	//releasing decode buffers
                	std::for_each(
                		(*it)->usedDecodeBuffers.begin(),
                		(*it)->usedDecodeBuffers.end(),
                		std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
                	(*it)->usedDecodeBuffers.clear();

                    //NOTE check for decoding errors is not supported by XVBA...
                    cl_log.log( QString::fromAscii("QnXVBADecoder. Surface is ready to be rendered"), cl_logDEBUG1 );
                    continue;   //checking surface once again (in case it is the only surface with decoded picture)
                }
                break;

            case XVBASurfaceContext::readyToRender:
#ifndef USE_OPENGL_SURFACE
                //can return only one decoded frame at a time. Choosing frame with lowest pts (with respect to overflow)
                //TODO/IMPL check pts for overflow
                if( !*decodedPicSurface || (*it)->pts < (*decodedPicSurface)->pts )
                    *decodedPicSurface = it->data();
                break;
#endif

            case XVBASurfaceContext::renderingInProgress:
            	//surface still being used by renderer
                break;
        }

        ++it;
    }

#ifdef USE_OPENGL_SURFACE
    for( std::list<QSharedPointer<XVBASurfaceContext> >::iterator
         it = m_glSurfaces.begin();
         it != m_glSurfaces.end();
         ++it )
    {
        switch( (*it)->state )
        {
        	case XVBASurfaceContext::readyToRender:
                //can return only one decoded frame at a time. Choosing frame with lowest pts (with respect to overflow)
                //TODO/IMPL check pts for overflow
                if( !*decodedPicSurface || (*it)->pts < (*decodedPicSurface)->pts )
                    *decodedPicSurface = *it;
                break;

            case XVBASurfaceContext::decodingInProgress:
           		NX_ASSERT( false );

            case XVBASurfaceContext::ready:
            case XVBASurfaceContext::renderingInProgress:
            	break;
        }
    }
#endif

    if( targetSurfaceCtx && !*targetSurfaceCtx && (m_commonSurfaces.size() < MAX_SURFACE_COUNT) && createSurface() )
        *targetSurfaceCtx = m_commonSurfaces.back();
}

QString QnXVBADecoder::lastErrorText() const
{
    switch( m_prevOperationStatus )
    {
        case Success:
            return QString::fromAscii("everything's okay");
        case BadRequest:
            return QString::fromAscii("bad request code");
        case BadValue:
            return QString::fromAscii("int parameter out of range");
        case BadWindow:
            return QString::fromAscii("parameter not a Window");
        case BadPixmap:
            return QString::fromAscii("parameter not a Pixmap");
        case BadAtom:
            return QString::fromAscii("parameter not an Atom");
        case BadCursor:
            return QString::fromAscii("parameter not a Cursor");
        case BadFont:
            return QString::fromAscii("parameter not a Font");
        case BadMatch:
            return QString::fromAscii("parameter mismatch");
        case BadDrawable:
            return QString::fromAscii("parameter not a Pixmap or Window");
        case BadAccess:
            return QString::fromAscii("bad access");
//                                             - key/button already grabbed
//                                             - attempt to free an illegal
//                                               cmap entry
//                                            - attempt to store into a read-only
//                                               color map entry.
//                                            - attempt to modify the access control
//                                               list from other than the local host.
        case BadAlloc:
            return QString::fromAscii("insufficient resources");
        case BadColor:
            return QString::fromAscii("no such colormap");
        case BadGC:
            return QString::fromAscii("parameter not a GC");
        case BadIDChoice:
            return QString::fromAscii("choice not in range or already used");
        case BadName:
            return QString::fromAscii("font or color name doesn't exist");
        case BadLength:
            return QString::fromAscii("Request length incorrect");
        case BadImplementation:
            return QString::fromAscii("server is defective");
        default:
            return QString::fromAscii("unknown error");
    }
}

QString QnXVBADecoder::decodeErrorToString( XVBA_DECODE_ERROR errorCode ) const
{
    switch( errorCode )
    {
        case XVBA_DECODE_NO_ERROR:
            return QString::fromAscii("NO_ERROR");
        case XVBA_DECODE_BAD_PICTURE:
            return QString::fromAscii("BAD_PICTURE");  //the entire picture is corrupted – all MBs are invalid
        case XVBA_DECODE_BAD_SLICE:
            return QString::fromAscii("BAD_SLICE");    //part of the picture, slice, wasn’t decoded properly – all MBs in this slice are bad
        case XVBA_DECODE_BAD_MB:
            return QString::fromAscii("BAD_MB");       //some MBs are not decoded properly
        default:
            return QString::fromAscii("Unknown error code %1").arg(errorCode);
    }
}

#include <QImage>

void QnXVBADecoder::fillOutputFrame( CLVideoDecoderOutput* const outFrame, QSharedPointer<XVBASurfaceContext> decodedPicSurface )
{
#ifdef SAVE_DECODED_FRAME_TO_FILE
	{
        static int fileNumber = 1;

        if( !glXMakeCurrent(
                m_display,
                QX11Info::appRootWindow(QX11Info::appScreen()),
                m_glContext ) )
        {
            GLenum errorCode = glGetError();
            std::cerr<<"Failed to make context current. "<<errorCode<<"\n";
        }

		glBindTexture( GL_TEXTURE_2D, decodedPicSurface->glTexture );
        glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, m_decodedPictureRgbaBuf );
		GLenum glErrorCode = glGetError();
		if( glErrorCode )
			std::cerr<<"Error ("<<glErrorCode<<") reading tex image\n";
        //we have RGBA32 in m_decodedPictureRgbaBuf, but need ARGB32 for QImage...
        for( quint32* curPixel = (quint32*)m_decodedPictureRgbaBuf;
             curPixel < (quint32*)(m_decodedPictureRgbaBuf + m_sps.pic_width_in_mbs*16*m_sps.getHeight()*4);
             ++curPixel )
        {
//            int r = *curPixel & 0xff;
//            int g = (*curPixel >> 8) & 0xff;
//            int b = (*curPixel >> 16) & 0xff;
//            int a = (*curPixel >> 24) & 0xff;

//            *curPixel = a | (b<<8) | (r<<16) | (g<<24);

//            quint32 alpha = *curPixel & 0xff;
//            *curPixel >>= 8;
//            *curPixel |= alpha << 24;
            quint32 alpha = *curPixel >> 24;
            *curPixel <<= 8;
            *curPixel |= alpha;
        }
		QImage img(
			m_decodedPictureRgbaBuf,
			m_sps.pic_width_in_mbs*16,
			m_sps.getHeight(),
            QImage::Format_ARGB32_Premultiplied );	//QImage::Format_ARGB4444_Premultiplied );
        const QString& fileName = QString::fromAscii("/home/ak/pic_dump/%1.bmp").arg(fileNumber++, 3, 10, QChar('0'));
		if( !img.save( fileName, "bmp" ) )
			std::cerr<<"Failed to save pic to "<<fileName.toStdString()<<"\n";

        if( !glXMakeCurrent(
                m_display,
                None,
                NULL ) )
        {
            GLenum errorCode = glGetError();
            std::cerr<<"Failed to reset current context. "<<errorCode<<"\n";
        }
	}
#endif

    outFrame->width = m_sps.pic_width_in_mbs*16;
    outFrame->height = m_sps.getHeight();

#ifdef USE_OPENGL_SURFACE
    NX_ASSERT( !decodedPicSurface || decodedPicSurface->state <= XVBASurfaceContext::renderingInProgress );
	outFrame->picData =	QSharedPointer<QnAbstractPictureData>( new QnXVBAOpenGLPictureData( decodedPicSurface ) );
#else
	XVBA_Get_Surface_Input getIn;
	memset( &getIn, 0, sizeof(getIn) );
	getIn.size = sizeof( getIn );
	getIn.session = m_decodeSession;
	getIn.src_surface = decodedPicSurface->surface;
	getIn.target_buffer = outFrame->data[0];
	getIn.target_width = outFrame->width;
	getIn.target_height = outFrame->height;
	getIn.target_pitch = outFrame->width / 2;
	getIn.target_parameter.size = sizeof(getIn.target_parameter);
	getIn.target_parameter.surfaceType = XVBA_NV12;
	getIn.target_parameter.flag = XVBA_FRAME;
	m_prevOperationStatus = XVBAGetSurface( &getIn );
	if( m_prevOperationStatus != Success )
	{
		cl_log.log( QString::fromAscii("QnXVBADecoder. Error reading surface. %1").arg(lastErrorText()), cl_logERROR );
	}

	outFrame->linesize[0] = getIn.target_pitch;

	//    if( !outFrame->isExternalData() )
//    {
//        //copying frame data if needed
//        outFrame->reallocate( m_sps.pic_width_in_mbs*16, m_sps.getHeight(), AV_PIX_FMT_NV12 );
//        memcpy( outFrame->data[0], decodedFrame->Data.Y, decodedFrame->Data.Pitch * m_sps.getHeight() );
//        memcpy( outFrame->data[1], decodedFrame->Data.U, decodedFrame->Data.Pitch / 2 * m_sps.getHeight() / 2 );
//        memcpy( outFrame->data[2], decodedFrame->Data.V, decodedFrame->Data.Pitch / 2 * m_sps.getHeight() / 2 );
//    }
//    else
//    {
//        outFrame->data[0] = decodedFrame->Data.Y;
//        outFrame->data[1] = decodedFrame->Data.U;
//        outFrame->data[2] = decodedFrame->Data.V;
//    }
//    if( pixelFormat == AV_PIX_FMT_NV12 )
//    {
//        outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
//        //outFrame->linesize[1] = decodedFrame->Data.Pitch;   //uv_stride
//        //outFrame->linesize[2] = 0;
//        outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //uv_stride
//        outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;
//    }
#endif

#ifdef USE_OPENGL_SURFACE
    outFrame->format = AV_PIX_FMT_RGBA;
#else
    outFrame->format = AV_PIX_FMT_NV12;
#endif
    //outFrame->format = pixelFormat;
    //outFrame->key_frame = decodedFrame->Data.
    //outFrame->pts = av_rescale_q( decodedFrame->Data.TimeStamp, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION, SRC_DATA_TMESTAMP_RESOLUTION );
    outFrame->pts = decodedPicSurface->pts;
    outFrame->pkt_dts = decodedPicSurface->pts;

//    outFrame->display_picture_number = decodedFrame->Data.FrameOrder;
    outFrame->interlaced_frame = 0;
//        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF ||
//        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF ||
//        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_REPEATED;
}

XVBABufferDescriptor* QnXVBADecoder::getDecodeBuffer( XVBA_BUFFER bufferType )
{
	vector<pair<XVBABufferDescriptor*, bool> >::size_type unusedBufIndex = 0;
	for( ;
		unusedBufIndex < m_xvbaDecodeBuffers.size();
		++unusedBufIndex )
	{
        if( !m_xvbaDecodeBuffers[unusedBufIndex].second && m_xvbaDecodeBuffers[unusedBufIndex].first->buffer_type == bufferType )
			break;
	}
	if( unusedBufIndex < m_xvbaDecodeBuffers.size() )
	{
		m_xvbaDecodeBuffers[unusedBufIndex].second = true;
        if( bufferType == XVBA_DATA_BUFFER )
            m_xvbaDecodeBuffers[unusedBufIndex].first->data_size_in_buffer = 0;
		return m_xvbaDecodeBuffers[unusedBufIndex].first;
	}

	XVBA_Create_DecodeBuff_Input in;
	memset( &in, 0, sizeof(in) );
	in.size = sizeof(in);
	in.session = m_decodeSession;
	in.buffer_type = bufferType;
    in.num_of_buffers = 1;

	XVBA_Create_DecodeBuff_Output out;
	memset( &out, 0, sizeof(out) );
	out.size = sizeof(out);

	m_prevOperationStatus = XVBACreateDecodeBuffers( &in, &out );
    if( (m_prevOperationStatus != Success) || (out.num_of_buffers_in_list == 0) )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error creating buffer of type %1: %2").arg(bufferType).arg(lastErrorText()), cl_logERROR );
        return NULL;
    }

    if( out.buffer_list->buffer_type == XVBA_PICTURE_DESCRIPTION_BUFFER )
    {
        if( out.buffer_list->buffer_size < sizeof(m_pictureDescriptor) )
        {
            cl_log.log( QString::fromAscii("QnXVBADecoder. Error creating buffer of type %1: Created buffer has size %2 while expected %3").
                arg(bufferType).arg(out.buffer_list->buffer_size).arg(sizeof(m_pictureDescriptor)), cl_logERROR );
            return NULL;
        }
//    	memcpy( out.buffer_list->bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
//    	out.buffer_list->data_size_in_buffer = sizeof(m_pictureglXMakeCurrentDescriptor);
    }
    else if( out.buffer_list->buffer_type == XVBA_DATA_CTRL_BUFFER )
    {
        out.buffer_list->data_size_in_buffer = sizeof(XVBADataCtrl);
    }

    out.buffer_list->data_offset = 0;

    cl_log.log( QString::fromAscii("QnXVBADecoder. Allocated xvba decode buffer of type %1, size %2").
        arg(bufferType).arg(out.buffer_list->buffer_size), cl_logDEBUG1 );

    m_xvbaDecodeBuffers.push_back( make_pair( out.buffer_list, false ) );

    if( bufferType == XVBA_DATA_BUFFER )
        m_xvbaDecodeBuffers.back().first->data_size_in_buffer = 0;
    m_xvbaDecodeBuffers.back().second = true;
	return m_xvbaDecodeBuffers.back().first;
}

void QnXVBADecoder::ungetBuffer( XVBABufferDescriptor* bufDescriptor )
{
	for( vector<pair<XVBABufferDescriptor*, bool> >::size_type i = 0;
		i < m_xvbaDecodeBuffers.size();
		++i )
	{
		if( m_xvbaDecodeBuffers[i].first == bufDescriptor )
		{
			m_xvbaDecodeBuffers[i].second = false;
			return;
        }
	}
}

void QnXVBADecoder::readSPS( const quint8* curNalu, const quint8* nextNalu )
{
	m_sps.decodeBuffer( curNalu, nextNalu );
	m_sps.deserialize();

    //reading frame cropping settings
    const unsigned int SubHeightC = m_sps.chroma_format_idc == 1 ? 2 : 1;
    const unsigned int CropUnitY = (m_sps.chroma_format_idc == 0)
        ? (2 - m_sps.frame_mbs_only_flag)
        : (SubHeightC * (2 - m_sps.frame_mbs_only_flag));
    m_originalFrameCropTop = CropUnitY * m_sps.frame_crop_top_offset;
    m_originalFrameCropBottom = CropUnitY * m_sps.frame_crop_bottom_offset;

    //--m_sps.pic_height_in_map_units;
#ifdef DROP_SMALL_SECOND_SLICE
    cl_log.log( QString::fromAscii("QnXVBADecoder::readSPS. Decreasing sps.pic_height_in_map_units by %1").arg(m_mbLinesToIgnore), cl_logDEBUG1 );
    m_sps.pic_height_in_map_units -= m_mbLinesToIgnore;
#endif

    m_pictureDescriptor.profile = h264ProfileIdcToXVBAProfile( m_sps.profile_idc );
	m_pictureDescriptor.level = m_sps.level_idc;
	m_pictureDescriptor.width_in_mb = m_sps.pic_width_in_mbs;
	m_pictureDescriptor.height_in_mb = m_sps.pic_height_in_map_units;
	m_pictureDescriptor.sps_info.avc.residual_colour_transform_flag = m_sps.residual_colour_transform_flag;
	m_pictureDescriptor.sps_info.avc.delta_pic_always_zero_flag = m_sps.delta_pic_order_always_zero_flag;
	m_pictureDescriptor.sps_info.avc.gaps_in_frame_num_value_allowed_flag = m_sps.gaps_in_frame_num_value_allowed_flag;
	m_pictureDescriptor.sps_info.avc.frame_mbs_only_flag = m_sps.frame_mbs_only_flag;
	m_pictureDescriptor.sps_info.avc.mb_adaptive_frame_field_flag = m_sps.mb_adaptive_frame_field_flag;
	m_pictureDescriptor.sps_info.avc.direct_8x8_inference_flag = m_sps.direct_8x8_inference_flag;

	m_pictureDescriptor.chroma_format = m_sps.chroma_format_idc;
	m_pictureDescriptor.avc_bit_depth_luma_minus8 = m_sps.bit_depth_luma;	// - 8;
	m_pictureDescriptor.avc_bit_depth_chroma_minus8 = m_sps.bit_depth_chroma;	// - 8;
	m_pictureDescriptor.avc_log2_max_frame_num_minus4 = m_sps.log2_max_frame_num - 4;

	m_pictureDescriptor.avc_pic_order_cnt_type = m_sps.pic_order_cnt_type;
	m_pictureDescriptor.avc_log2_max_pic_order_cnt_lsb_minus4 = m_sps.log2_max_pic_order_cnt_lsb - 4;
	m_pictureDescriptor.avc_num_ref_frames = m_sps.num_ref_frames;

    m_h264PocData.MaxFrameNum = 1 << m_sps.log2_max_frame_num;
}

void QnXVBADecoder::readPPS( const quint8* curNalu, const quint8* nextNalu )
{
	m_pps.decodeBuffer( curNalu, nextNalu );
	m_pps.deserialize();

	m_pictureDescriptor.avc_num_slice_groups_minus1 = m_pps.num_slice_groups_minus1;
	m_pictureDescriptor.avc_slice_group_map_type = m_pps.slice_group_map_type;
	m_pictureDescriptor.avc_num_ref_idx_l0_active_minus1 = m_pps.num_ref_idx_l0_active_minus1;
	m_pictureDescriptor.avc_num_ref_idx_l1_active_minus1 = m_pps.num_ref_idx_l1_active_minus1;

	m_pictureDescriptor.avc_pic_init_qp_minus26 = m_pps.pic_init_qp_minus26;
	m_pictureDescriptor.avc_pic_init_qs_minus26 = m_pps.pic_init_qs_minus26;
	m_pictureDescriptor.avc_chroma_qp_index_offset = m_pps.chroma_qp_index_offset;
    m_pictureDescriptor.avc_second_chroma_qp_index_offset = /*-2;*/	m_pps.second_chroma_qp_index_offset;

    m_pictureDescriptor.avc_slice_group_change_rate_minus1 = /*0;*/	m_pps.slice_group_change_rate;	// - 1;

    //*(unsigned int*)(&m_pictureDescriptor.pps_info.avc) = 32;
    m_pictureDescriptor.pps_info.avc.entropy_coding_mode_flag = m_pps.entropy_coding_mode_flag;
    m_pictureDescriptor.pps_info.avc.pic_order_present_flag = m_pps.pic_order_present_flag;
    m_pictureDescriptor.pps_info.avc.weighted_pred_flag = m_pps.weighted_pred_flag;
    m_pictureDescriptor.pps_info.avc.weighted_bipred_idc = m_pps.weighted_bipred_idc;
    m_pictureDescriptor.pps_info.avc.deblocking_filter_control_present_flag = m_pps.deblocking_filter_control_present_flag;
    m_pictureDescriptor.pps_info.avc.constrained_intra_pred_flag = m_pps.constrained_intra_pred_flag;
    m_pictureDescriptor.pps_info.avc.redundant_pic_cnt_present_flag = m_pps.redundant_pic_cnt_present_flag;
    m_pictureDescriptor.pps_info.avc.transform_8x8_mode_flag = m_pps.transform_8x8_mode_flag;
}

QnXVBADecoder::XVBASurfaceContext* QnXVBADecoder::findUnusedGLSurface()
{
    for( std::list<QSharedPointer<XVBASurfaceContext> >::iterator
         it = m_glSurfaces.begin();
         it != m_glSurfaces.end();
         ++it )
    {
    	if( (*it)->state == XVBASurfaceContext::ready )
    		return it->data();
    }

    createGLSurface();
    return m_glSurfaces.back().data();
}

void QnXVBADecoder::calcH264POC( SliceEx* const pSlice )
{
    //code is taken from JM reference decoder

    int i = 0;
    // for POC mode 0:
    unsigned int MaxPicOrderCntLsb = 1<<(m_sps.log2_max_pic_order_cnt_lsb);

    switch ( m_sps.pic_order_cnt_type )
    {
    case 0: // POC MODE 0
        // 1st
        if(pSlice->nal_unit_type == nuSliceIDR)
        {
            m_h264PocData.PrevPicOrderCntMsb = 0;
            m_h264PocData.PrevPicOrderCntLsb = 0;
        }
        else
        {
            if (m_h264PocData.last_has_mmco_5)
            {
                if (m_h264PocData.last_pic_bottom_field)
                {
                    m_h264PocData.PrevPicOrderCntMsb = 0;
                    m_h264PocData.PrevPicOrderCntLsb = 0;
                }
                else
                {
                    m_h264PocData.PrevPicOrderCntMsb = 0;
                    m_h264PocData.PrevPicOrderCntLsb = pSlice->toppoc;
                }
            }
        }
        // Calculate the MSBs of current picture
        if( pSlice->pic_order_cnt_lsb  <  m_h264PocData.PrevPicOrderCntLsb  &&
                ( m_h264PocData.PrevPicOrderCntLsb - pSlice->pic_order_cnt_lsb )  >=  ( MaxPicOrderCntLsb / 2 ) )
            pSlice->PicOrderCntMsb = m_h264PocData.PrevPicOrderCntMsb + MaxPicOrderCntLsb;
        else if ( pSlice->pic_order_cnt_lsb  >  m_h264PocData.PrevPicOrderCntLsb  &&
                  ( pSlice->pic_order_cnt_lsb - m_h264PocData.PrevPicOrderCntLsb )  >  ( MaxPicOrderCntLsb / 2 ) )
            pSlice->PicOrderCntMsb = m_h264PocData.PrevPicOrderCntMsb - MaxPicOrderCntLsb;
        else
            pSlice->PicOrderCntMsb = m_h264PocData.PrevPicOrderCntMsb;

        // 2nd

        if(pSlice->m_field_pic_flag==0)
        {           //frame pix
            pSlice->toppoc = pSlice->PicOrderCntMsb + pSlice->pic_order_cnt_lsb;
            pSlice->bottompoc = pSlice->toppoc + pSlice->delta_pic_order_cnt_bottom;
            pSlice->ThisPOC = pSlice->framepoc = (pSlice->toppoc < pSlice->bottompoc)? pSlice->toppoc : pSlice->bottompoc; // POC200301
        }
        else if (pSlice->bottom_field_flag == FALSE)
        {  //top field
            pSlice->ThisPOC= pSlice->toppoc = pSlice->PicOrderCntMsb + pSlice->pic_order_cnt_lsb;
        }
        else
        {  //bottom field
            pSlice->ThisPOC= pSlice->bottompoc = pSlice->PicOrderCntMsb + pSlice->pic_order_cnt_lsb;
        }
        pSlice->framepoc = pSlice->ThisPOC;

        m_h264PocData.ThisPOC = pSlice->ThisPOC;

        if ( pSlice->frame_num != m_h264PocData.PreviousFrameNum)
            m_h264PocData.PreviousFrameNum = pSlice->frame_num;

        if(pSlice->nal_ref_idc)
        {
            m_h264PocData.PrevPicOrderCntLsb = pSlice->pic_order_cnt_lsb;
            m_h264PocData.PrevPicOrderCntMsb = pSlice->PicOrderCntMsb;
        }
        break;

    case 1: // POC MODE 1
        // 1st
        if(pSlice->nal_unit_type == nuSliceIDR)
        {
            m_h264PocData.FrameNumOffset=0;     //  first pix of IDRGOP,
            pSlice->delta_pic_order_cnt[0]=0;                        //ignore first delta
            if(pSlice->frame_num)
                cl_log.log("frame_num not equal to zero in IDR picture", cl_logERROR);
        }
        else
        {
            if (m_h264PocData.last_has_mmco_5)
            {
                m_h264PocData.PreviousFrameNumOffset = 0;
                m_h264PocData.PreviousFrameNum = 0;
            }
            if (pSlice->frame_num<m_h264PocData.PreviousFrameNum)
            {             //not first pix of IDRGOP
                m_h264PocData.FrameNumOffset = m_h264PocData.PreviousFrameNumOffset + m_h264PocData.MaxFrameNum;
            }
            else
            {
                m_h264PocData.FrameNumOffset = m_h264PocData.PreviousFrameNumOffset;
            }
        }

        // 2nd
        if(m_sps.num_ref_frames_in_pic_order_cnt_cycle)
            pSlice->AbsFrameNum = m_h264PocData.FrameNumOffset+pSlice->frame_num;
        else
            pSlice->AbsFrameNum=0;
        if( (!pSlice->nal_ref_idc) && pSlice->AbsFrameNum > 0)
            pSlice->AbsFrameNum--;

        // 3rd
        m_h264PocData.ExpectedDeltaPerPicOrderCntCycle=0;
        if(m_sps.num_ref_frames_in_pic_order_cnt_cycle)
            for(i=0;i<(int) m_sps.num_ref_frames_in_pic_order_cnt_cycle;i++)
                m_h264PocData.ExpectedDeltaPerPicOrderCntCycle += m_sps.offset_for_ref_frame[i];

        if(pSlice->AbsFrameNum)
        {
            m_h264PocData.PicOrderCntCycleCnt = (pSlice->AbsFrameNum-1)/m_sps.num_ref_frames_in_pic_order_cnt_cycle;
            m_h264PocData.FrameNumInPicOrderCntCycle = (pSlice->AbsFrameNum-1)%m_sps.num_ref_frames_in_pic_order_cnt_cycle;
            m_h264PocData.ExpectedPicOrderCnt = m_h264PocData.PicOrderCntCycleCnt*m_h264PocData.ExpectedDeltaPerPicOrderCntCycle;
            for(i=0;i<=(int)m_h264PocData.FrameNumInPicOrderCntCycle;i++)
                m_h264PocData.ExpectedPicOrderCnt += m_sps.offset_for_ref_frame[i];
        }
        else
            m_h264PocData.ExpectedPicOrderCnt=0;

        if(!pSlice->nal_ref_idc)
            m_h264PocData.ExpectedPicOrderCnt += m_sps.offset_for_non_ref_pic;

        if(pSlice->m_field_pic_flag==0)
        {           //frame pix
            pSlice->toppoc = m_h264PocData.ExpectedPicOrderCnt + pSlice->delta_pic_order_cnt[0];
            pSlice->bottompoc = pSlice->toppoc + m_sps.offset_for_top_to_bottom_field + pSlice->delta_pic_order_cnt[1];
            pSlice->ThisPOC = pSlice->framepoc = (pSlice->toppoc < pSlice->bottompoc)? pSlice->toppoc : pSlice->bottompoc; // POC200301
        }
        else if (pSlice->bottom_field_flag == FALSE)
        {  //top field
            pSlice->ThisPOC = pSlice->toppoc = m_h264PocData.ExpectedPicOrderCnt + pSlice->delta_pic_order_cnt[0];
        }
        else
        {  //bottom field
            pSlice->ThisPOC = pSlice->bottompoc = m_h264PocData.ExpectedPicOrderCnt + m_sps.offset_for_top_to_bottom_field + pSlice->delta_pic_order_cnt[0];
        }
        pSlice->framepoc=pSlice->ThisPOC;

        m_h264PocData.PreviousFrameNum=pSlice->frame_num;
        m_h264PocData.PreviousFrameNumOffset=m_h264PocData.FrameNumOffset;
        break;

    case 2: // POC MODE 2
        if(pSlice->nal_unit_type == nuSliceIDR) // IDR picture
        {
            m_h264PocData.FrameNumOffset=0;     //  first pix of IDRGOP,
            pSlice->ThisPOC = pSlice->framepoc = pSlice->toppoc = pSlice->bottompoc = 0;
            if(pSlice->frame_num)
                cl_log.log("frame_num not equal to zero in IDR picture", cl_logERROR);
        }
        else
        {
            if (m_h264PocData.last_has_mmco_5)
            {
                m_h264PocData.PreviousFrameNum = 0;
                m_h264PocData.PreviousFrameNumOffset = 0;
            }
            if (pSlice->frame_num<m_h264PocData.PreviousFrameNum)
                m_h264PocData.FrameNumOffset = m_h264PocData.PreviousFrameNumOffset + m_h264PocData.MaxFrameNum;
            else
                m_h264PocData.FrameNumOffset = m_h264PocData.PreviousFrameNumOffset;

            pSlice->AbsFrameNum = m_h264PocData.FrameNumOffset+pSlice->frame_num;
            if(!pSlice->nal_ref_idc)
                pSlice->ThisPOC = (2*pSlice->AbsFrameNum - 1);
            else
                pSlice->ThisPOC = (2*pSlice->AbsFrameNum);

            if (pSlice->m_field_pic_flag==0)
                pSlice->toppoc = pSlice->bottompoc = pSlice->framepoc = pSlice->ThisPOC;
            else if (pSlice->bottom_field_flag == FALSE)
                pSlice->toppoc = pSlice->framepoc = pSlice->ThisPOC;
            else
                pSlice->bottompoc = pSlice->framepoc = pSlice->ThisPOC;
        }

        m_h264PocData.PreviousFrameNum = pSlice->frame_num;
        m_h264PocData.PreviousFrameNumOffset = m_h264PocData.FrameNumOffset;
        break;

    default:
        //error must have occured
        break;
    }
}

void QnXVBADecoder::destroyXVBASession()
{
    if( !m_decodeSession )
        return;

    foreach( QSharedPointer<XVBASurfaceContext> surfaceCtx, m_glSurfaces )
    {
        std::for_each(
            surfaceCtx->usedDecodeBuffers.begin(),
            surfaceCtx->usedDecodeBuffers.end(),
            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        surfaceCtx->usedDecodeBuffers.clear();
        surfaceCtx->clear();
    }
    m_glSurfaces.clear();

    foreach( QSharedPointer<XVBASurfaceContext> surfaceCtx, m_commonSurfaces )
    {
        std::for_each(
            surfaceCtx->usedDecodeBuffers.begin(),
            surfaceCtx->usedDecodeBuffers.end(),
            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        surfaceCtx->usedDecodeBuffers.clear();
        surfaceCtx->clear();
    }
    m_commonSurfaces.clear();

    //deleting decode buffers
    XVBA_Destroy_Decode_Buffers_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    in.session = m_decodeSession;
    in.num_of_buffers_in_list = 1;
    for( size_t i = 0; i < m_xvbaDecodeBuffers.size(); ++i )
    {
        in.buffer_list = m_xvbaDecodeBuffers[i].first;
        m_prevOperationStatus = XVBADestroyDecodeBuffers( &in );
        if( m_prevOperationStatus != Success )
            cl_log.log( QString::fromAscii("QnXVBADecoder. Could not destroy decode buffer. %1").arg(lastErrorText()), cl_logERROR );
    }
    m_xvbaDecodeBuffers.clear();

    m_prevOperationStatus = XVBADestroyDecode( m_decodeSession );
    if( m_prevOperationStatus != Success )
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not destroy decode session. %1").arg(lastErrorText()), cl_logERROR );
    m_decodeSession = NULL;
}

Status QnXVBADecoder::XVBADecodePicture( XVBA_Decode_Picture_Input* decode_picture_input )
{
#ifdef DUMP_DECODER_INPUT
#ifndef DUMP_BY_SLICES
    //assuming XVBA_DATA_BUFFER precedes XVBA_DATA_CTRL_BUFFER
    const XVBABufferDescriptor* dataBufferDescriptor = decode_picture_input->buffer_list[0];
    NX_ASSERT( decode_picture_input->buffer_list[0]->buffer_type == XVBA_DATA_BUFFER );
    for( size_t i = 1; i < decode_picture_input->num_of_buffers_in_list; ++i )
    {
        NX_ASSERT( decode_picture_input->buffer_list[i]->buffer_type == XVBA_DATA_CTRL_BUFFER );
        const XVBADataCtrl* ctrl = reinterpret_cast<const XVBADataCtrl*>( decode_picture_input->buffer_list[i]->bufferXVBA );
        m_inStream.write( (const char*)dataBufferDescriptor->bufferXVBA + ctrl->SliceDataLocation, ctrl->SliceBytesInBuffer );
    }
#else
    {
        static int fileNumber = 1;
        char outFilePath[256];

        for( size_t i = 2; i < targetSurfaceCtx->usedDecodeBuffers.size(); ++i )
        {
            sprintf( outFilePath, "/home/ak/slices/%d", fileNumber++ );
            std::ofstream of( outFilePath, std::ios_base::binary );
            of.write(
                (const char*)dataBufferDescriptor->bufferXVBA + dataBufferDescriptor->data_offset + ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceDataLocation,
                ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceBytesInBuffer );
//    		m_inStream.write(
//    			(const char*)dataBufferDescriptor->bufferXVBA + dataBufferDescriptor->data_offset + ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceDataLocation,
//				((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceBytesInBuffer );
        }
    }
#endif
#endif

    return ::XVBADecodePicture( decode_picture_input );
}

#ifdef JOIN_SLICES
void QnXVBADecoder::appendSlice( XVBABufferDescriptor* leftSliceBuffer, const quint8* rightSliceNALU, size_t rightSliceNALUSize )
{
    NX_ASSERT( leftSliceBuffer->data_size_in_buffer > 0 );
    NX_ASSERT( !m_pps.entropy_coding_mode_flag );

    cl_log.log( QString::fromAscii("QnXVBADecoder. Appending slice. %1").arg(lastErrorText()), cl_logDEBUG1 );

    // in h.264 stream left bit is MSB, right bit - LSB

	//TODO/IMPL
    //parsing leftSliceBuffer from end to beginning to find start of rbsp_trailing_bits, skipping cabac_zero_word, if entropy_encoding
    quint8* leftSliceDataLastBytePos = static_cast<quint8*>(leftSliceBuffer->bufferXVBA) + leftSliceBuffer->data_size_in_buffer - 1;
    int leftSliceDataLastBitPos = 0;
    for( ;
         leftSliceDataLastBytePos > leftSliceBuffer->bufferXVBA;
         --leftSliceDataLastBytePos )
    {
        int byteVal = *leftSliceDataLastBytePos;
        if( byteVal == 0 )
            continue;
        //skipping rbsp_trailing_bits(): searching first one-bit from right
        for( ; ((byteVal >> leftSliceDataLastBitPos) & 1) == 0; ++leftSliceDataLastBitPos ) {}
//        if( leftSliceDataLastBitPos == 7 )
//        {
//            leftSliceDataLastBitPos = 0;
//            --leftSliceDataLastBytePos;
//        }
        break;
    }

    dumpBuffer( QString::fromAscii("Left slice ending. Last bit pos %1").arg(leftSliceDataLastBitPos), leftSliceDataLastBytePos - 3, 4 );

    //leftSliceDataLastBitPos points to rbsp_trailing_bits

    //TODO if entropy_encoding, setting end_of_slice_flag to zero

    //skipping all in rightSlice up to macroblock loop
    SliceUnit sliceHeader;
    const quint8* rightSliceNALUStart = NALUnit::findNextNAL( rightSliceNALU, rightSliceNALU+rightSliceNALUSize );
    sliceHeader.decodeBuffer( rightSliceNALUStart, rightSliceNALU+rightSliceNALUSize );
    sliceHeader.deserialize( &m_sps, &m_pps );
    //calculating offset to right slice slice_data() (no CABAC!). Assuming slice_header() has no emulation_prevention_three_byte!
    const size_t bitsToRightMacroblockLoop = sliceHeader.getBitReader().getBitsCount() + 8;  //8 - NALU header size
    const size_t bytesToRightMacroblockLoop = bitsToRightMacroblockLoop >> 3;
    //copying rightSlice macroblocks next to leftSlice
    memcpy(
        leftSliceDataLastBytePos + 1,
        rightSliceNALU + bytesToRightMacroblockLoop,
        rightSliceNALUSize - bytesToRightMacroblockLoop );

    dumpBuffer( QString::fromAscii("Right slice slice_data() start. (first bit pos %1)").arg(7-(bitsToRightMacroblockLoop & 0x07)),
                rightSliceNALU + bytesToRightMacroblockLoop, 4 );

    //moving rightSlice bits (if needed)
    moveBits(
        leftSliceDataLastBytePos,
        8 + (bitsToRightMacroblockLoop & 0x07),
        7-leftSliceDataLastBitPos,
        ((rightSliceNALUSize - bytesToRightMacroblockLoop) << 3) + (8 - (bitsToRightMacroblockLoop & 0x07)) );

    dumpBuffer( QString::fromAscii("Merged slice"), leftSliceDataLastBytePos-1, 4 );

    //new slice already closed by ending of right slice
}

void QnXVBADecoder::dumpBuffer( const QString& msg, const quint8* buf, size_t bufSize )
{
    qDebug()<<msg;
    QString bitStr;
    for( size_t i = 0; i < bufSize; ++i )
    {
        for( int bitPos = 7; bitPos >= 0; --bitPos )
            bitStr += QString::number((buf[i]>>bitPos) & 1);
        bitStr += QString::fromAscii(" ");
    }

    qDebug()<<bitStr;
}
#endif

int QnXVBADecoder::h264ProfileIdcToXVBAProfile( int profile_idc )
{
    switch( profile_idc )
    {
        case 66:
            return 1;
        case 77:
            return 2;
        case 100:
            return 3;
        default:
            return 3;	//high
    }
}
