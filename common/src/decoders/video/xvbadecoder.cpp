////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include <QDebug>

#include "xvbadecoder.h"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>

#include <QElapsedTimer>
#include <QX11Info>

//decode to opengl texture
#define USE_OPENGL_SURFACE
//each XVBADecode call gets only one slice of primary coded picture and same surface
//#define DECODE_PIC_SLICES_SEPARATELY
//#define SAVE_DECODED_FRAME_TO_FILE
#define DUMP_DECODER_INPUT
#ifdef DUMP_DECODER_INPUT
//#define DUMP_BY_SLICES
#endif
#define DIGITAL_WATCHDOG_DEBUGGING


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
QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData( XVBASurfaceContext* ctx )
:
    QnOpenGLPictureData(
//		ctx->glContext,
		ctx->glTexture ),
    m_ctx( ctx )
{
    Q_ASSERT_X(
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
QnXVBADecoder::QnXVBADecoder( const QGLContext* const glContext, const QnCompressedVideoDataPtr& data )
:
    m_prevOperationStatus( Success ),
    m_context( NULL ),
    m_decodeSession( NULL ),
    m_glContext( NULL ),
    m_display( NULL ),
    m_getCapDecodeOutputSize( 0 ),
    m_decodedPictureRgbaBuf( NULL ),
    m_decodedPictureRgbaBufSize( 0 ),
    m_totalSurfacesBeingDecoded( 0 )
#ifdef XVBA_TEST
    ,m_hardwareAccelerationEnabled( false )
#endif
{
    cl_log.log( QString::fromAscii("Initializing XVBA decoder"), cl_logDEBUG1 );

    int version = 0;
    m_display = QX11Info::display();    //retrieving display pointer
    m_hardwareAccelerationEnabled = XVBAQueryExtension( m_display, &version );
    if( !m_hardwareAccelerationEnabled )
    {
        cl_log.log( QString::fromAscii("XVBA decode acceleration is not supported on host system"), cl_logERROR );
        return;
    }
    cl_log.log( QString::fromAscii("XVBA of version %1.%2 found").arg(version >> 16).arg(version & 0xffff), cl_logINFO );

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
    destroyXVBASession();

    if( m_context )
    {
        XVBADestroyContext( m_context );
        m_context = NULL;
    }

    if( m_glContext )
        glXDestroyContext( m_display, m_glContext );

#ifdef SAVE_DECODED_FRAME_TO_FILE
    delete[] m_decodedPictureRgbaBuf;
    m_decodedPictureRgbaBuf = NULL;
#endif
}

//!Implementation of AbstractDecoder::GetPixelFormat
PixelFormat QnXVBADecoder::GetPixelFormat() const
{
#ifdef USE_OPENGL_SURFACE
    return PIX_FMT_RGBA;
#else
    return PIX_FMT_NV12;
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

//!Implementation of AbstractDecoder::decode
bool QnXVBADecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
//#ifdef DUMP_DECODER_INPUT
//  	m_inStream.write( data->data.data(), data->data.size() );
//#endif

    //NOTE decoder may use more than one surface at a time. We must not block until picture decoding is finished,
        //since decoder may require some more data to finish picture decoding
        //We can just remember, that this frame with pts is being decoded to this surface

    Q_ASSERT( m_hardwareAccelerationEnabled );
    Q_ASSERT( m_context );

    if( !m_decodeSession && !createDecodeSession() )
        return false;

    //checking, if one of surfaces is done already
    XVBASurfaceContext* targetSurfaceCtx = NULL;
    XVBASurfaceContext* decodedPicSurface = NULL;
    checkSurfaces( &targetSurfaceCtx, &decodedPicSurface );
    Q_ASSERT( !decodedPicSurface || decodedPicSurface->state <= XVBASurfaceContext::renderingInProgress );

    if( decodedPicSurface )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Found decoded picture (pts %1). Providing it to output...").arg(decodedPicSurface->pts), cl_logDEBUG1 );
        //copying picture to output
    	fillOutputFrame( outFrame, decodedPicSurface );
        decodedPicSurface->state = XVBASurfaceContext::renderingInProgress;
    }

    if( !targetSurfaceCtx )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find unused gl surface. Total surface number: %1").arg(m_commonSurfaces.size()), cl_logWARNING );
        return decodedPicSurface != NULL;
    }

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    //TODO/IMPL in case of interlaced video, possibly we'll have to split incoming frame to two fields

    XVBABufferDescriptor* pictureDescriptionBufferDescriptor = getDecodeBuffer( XVBA_PICTURE_DESCRIPTION_BUFFER );
    if( !pictureDescriptionBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_PICTURE_DESCRIPTION_BUFFER buffer"), cl_logERROR );
        return decodedPicSurface != NULL;
    }
	memcpy( pictureDescriptionBufferDescriptor->bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
	pictureDescriptionBufferDescriptor->data_size_in_buffer = sizeof(m_pictureDescriptor);
    targetSurfaceCtx->usedDecodeBuffers.push_back( pictureDescriptionBufferDescriptor );
    m_pictureDescriptorBufArray[0] = pictureDescriptionBufferDescriptor;
    m_pictureDescriptorDecodeInput.num_of_buffers_in_list = 1;

	XVBABufferDescriptor* qmBufferDescriptor = getDecodeBuffer( XVBA_QM_BUFFER );
    if( !qmBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_QM_BUFFER buffer"), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        return decodedPicSurface != NULL;
    }
    XVBAQuantMatrixAvc* quantMatrixAVC = static_cast<XVBAQuantMatrixAvc*>( qmBufferDescriptor->bufferXVBA );

    memset( quantMatrixAVC->bScalingLists4x4, 16, sizeof(quantMatrixAVC->bScalingLists4x4) );
    memset( quantMatrixAVC->bScalingLists8x8, 16, sizeof(quantMatrixAVC->bScalingLists8x8) );

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

	XVBABufferDescriptor* dataBufferDescriptor = getDecodeBuffer( XVBA_DATA_BUFFER );
    if( !dataBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_BUFFER buffer"), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        return decodedPicSurface != NULL;
    }
    const size_t buffersBeforeDataBuffer = targetSurfaceCtx->usedDecodeBuffers.size();
    targetSurfaceCtx->usedDecodeBuffers.push_back( dataBufferDescriptor );

    if( !analyzeInputBufSlices(
    		data,
    		static_cast<XVBAPictureDescriptor*>(pictureDescriptionBufferDescriptor->bufferXVBA),
            &targetSurfaceCtx->usedDecodeBuffers ) )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find slice in source data"), cl_logWARNING );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        return decodedPicSurface != NULL;
    }

#ifdef DIGITAL_WATCHDOG_DEBUGGING
    if( targetSurfaceCtx->usedDecodeBuffers.size() - (buffersBeforeDataBuffer+1) > 1 )
    {
        //leaving only first slice of picture
//        std::vector<XVBABufferDescriptor*>::iterator firstDataCtrlToEraseIter =
//                targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1+1;
//        std::for_each(
//            firstDataCtrlToEraseIter,
//            targetSurfaceCtx->usedDecodeBuffers.end(),
//            std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
//        targetSurfaceCtx->usedDecodeBuffers.erase( firstDataCtrlToEraseIter, targetSurfaceCtx->usedDecodeBuffers.end() );

        //leaving only last slice of picture
        ungetBuffer( *(targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1) );
        targetSurfaceCtx->usedDecodeBuffers.erase( targetSurfaceCtx->usedDecodeBuffers.begin() + buffersBeforeDataBuffer+1 );
    }
#endif

    Q_ASSERT( targetSurfaceCtx->state == XVBASurfaceContext::ready );
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
        return decodedPicSurface != NULL;
    }

    //at this moment XVBADataCtrl buffers found in targetSurfaceCtx->usedDecodeBuffers point to slices in source buffer (data->data), not in XVBA_DATA_BUFFER

    //copying coded data to data buffer
    char* dataStart = reinterpret_cast<char*>(dataBufferDescriptor->bufferXVBA);
    for( size_t i = buffersBeforeDataBuffer+1; i < targetSurfaceCtx->usedDecodeBuffers.size(); ++i )
    {
#ifdef DECODE_PIC_SLICES_SEPARATELY
        if( i > buffersBeforeDataBuffer+1 )
        {
            dataBufferDescriptor = getDecodeBuffer( XVBA_DATA_BUFFER );
            if( !dataBufferDescriptor )
            {
                cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_BUFFER buffer"), cl_logERROR );
                std::for_each(
                    targetSurfaceCtx->usedDecodeBuffers.begin(),
                    targetSurfaceCtx->usedDecodeBuffers.end(),
                    std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
                return decodedPicSurface != NULL;
            }
            dataStart = reinterpret_cast<char*>(dataBufferDescriptor->bufferXVBA);
            targetSurfaceCtx->usedDecodeBuffers.insert( targetSurfaceCtx->usedDecodeBuffers.begin()+i, dataBufferDescriptor );
            ++i;
        }
#endif

        XVBADataCtrl* ctrl = ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA);

    	memcpy( dataStart + dataBufferDescriptor->data_size_in_buffer,
    			data->data.data() + ctrl->SliceDataLocation,
    			ctrl->SliceBytesInBuffer );
    	ctrl->SliceDataLocation = dataBufferDescriptor->data_size_in_buffer;
    	dataBufferDescriptor->data_size_in_buffer += ctrl->SliceBytesInBuffer;

        //adding two trailing zeros, as required by XVBA
        static const size_t TRAILING_ZEROS_COUNT = 2;
        memset( dataStart + dataBufferDescriptor->data_size_in_buffer, 0, TRAILING_ZEROS_COUNT );
        dataBufferDescriptor->data_size_in_buffer += TRAILING_ZEROS_COUNT;
        ctrl->SliceBytesInBuffer += TRAILING_ZEROS_COUNT;
        ctrl->SliceBitsInBuffer += TRAILING_ZEROS_COUNT << 3;
    }

//#ifndef DECODE_PIC_SLICES_SEPARATELY
//    size_t data_size_in_buffer_bak = dataBufferDescriptor->data_size_in_buffer;
//    dataBufferDescriptor->data_size_in_buffer = ALIGN128(dataBufferDescriptor->data_size_in_buffer);
//    memset( dataStart + bak, 0, dataBufferDescriptor->data_size_in_buffer - bak );
//#endif

#ifdef DECODE_PIC_SLICES_SEPARATELY
    const size_t totalDataBuffers = (targetSurfaceCtx->usedDecodeBuffers.size() - buffersBeforeDataBuffer) / 2;
    for( size_t i = 0; i < totalDataBuffers; ++i )
    {
        m_srcDataDecodeInput.buffer_list = &targetSurfaceCtx->usedDecodeBuffers[buffersBeforeDataBuffer+i*2];	//index 0 - pictureDescriptionBufferDescriptor, 1 - quantization matrix buffer which is not needed here
        m_srcDataDecodeInput.num_of_buffers_in_list = 2;
        dataBufferDescriptor = targetSurfaceCtx->usedDecodeBuffers[buffersBeforeDataBuffer+i*2];
#else
        m_srcDataDecodeInput.buffer_list = &targetSurfaceCtx->usedDecodeBuffers[buffersBeforeDataBuffer];	//index 0 - pictureDescriptionBufferDescriptor, 1 - quantization matrix buffer which is not needed here
        m_srcDataDecodeInput.num_of_buffers_in_list = targetSurfaceCtx->usedDecodeBuffers.size()-buffersBeforeDataBuffer;
#endif

        m_prevOperationStatus = XVBADecodePicture( &m_srcDataDecodeInput );
        if( m_prevOperationStatus != Success )
        {
            cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving src data to decoder. %1. buf %2, bufsize %3").
                arg(lastErrorText()).arg((size_t)dataStart, 0, 16).arg(dataBufferDescriptor->buffer_size, 0, 16), cl_logERROR );
            std::for_each(
                targetSurfaceCtx->usedDecodeBuffers.begin(),
                targetSurfaceCtx->usedDecodeBuffers.end(),
                std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
            return decodedPicSurface != NULL;
        }
#ifdef DECODE_PIC_SLICES_SEPARATELY
    }   //for
#endif

    m_prevOperationStatus = XVBAEndDecodePicture( &m_decodeEndInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving Decode_Picture_End to decoder. %1").arg(lastErrorText()), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
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
void QnXVBADecoder::setOutPictureSize( const QSize& outSize )
{
    m_outPicSize = outSize;
}

#ifndef XVBA_TEST
void QnXVBADecoder::setLightCpuMode( QnAbstractVideoDecoder::DecodeMode )
{
    //TODO/IMPL
}
#endif

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

        if( ((out.decode_caps_list[foundCapIndex].flags & XVBA_H264_BASELINE) > 0 && m_sps.profile_idc == H264Profile::baseline) ||
            ((out.decode_caps_list[foundCapIndex].flags & XVBA_H264_MAIN) > 0 && m_sps.profile_idc == H264Profile::main) ||
            ((out.decode_caps_list[foundCapIndex].flags & XVBA_H264_HIGH) > 0 && m_sps.profile_idc >= H264Profile::high) )
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
        in.height = m_sps.pic_height_in_map_units*16;
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

    return m_prevOperationStatus == Success;
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
                  m_sps.pic_height_in_map_units*16,
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
       		arg(m_sps.pic_width_in_mbs*16).arg(m_sps.pic_height_in_map_units*16).arg(glErrorCode), cl_logERROR );
    }
    cl_log.log( QString::fromAscii("Created opengl texture %1x%2").arg(m_sps.pic_width_in_mbs*16).arg(m_sps.pic_height_in_map_units*16), cl_logDEBUG1 );

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
    in.height = m_pictureDescriptor.height_in_mb * 16;
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
    cl_log.log( QString::fromAscii("XVBA surface created successfully. Total surface count: %1" ).arg(m_commonSurfaces.size()), cl_logDEBUG1 );
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
        Q_ASSERT( nextNalu > curNalu );
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
        Q_ASSERT( nextNalu > curNalu );
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
                    break;

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
                pictureDescriptor->avc_intra_flag = slice.slice_type == SliceUnit::I_TYPE ? 1 : 0;    //I - frame. Supposing, that only I frames are intra coded
                pictureDescriptor->avc_reference = slice.nal_ref_idc > 0 ? 1 : 0;
            }

			case nuSliceB:
            case nuSliceC:
            case nuSliceWithoutPartitioning:
            {
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

    return dataCtrlBuffers->size() > sizeBak;
}

void QnXVBADecoder::checkSurfaces( XVBASurfaceContext** const targetSurfaceCtx, XVBASurfaceContext** const decodedPicSurface )
{
    for( std::list<QSharedPointer<XVBASurfaceContext> >::iterator
         it = m_commonSurfaces.begin();
         it != m_commonSurfaces.end();
          )
    {
        switch( (*it)->state )
        {
            case XVBASurfaceContext::ready:
                if( !*targetSurfaceCtx )
                    *targetSurfaceCtx = it->data();
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
                    *decodedPicSurface = it->data();
                break;

            case XVBASurfaceContext::decodingInProgress:
           		Q_ASSERT( false );

            case XVBASurfaceContext::ready:
            case XVBASurfaceContext::renderingInProgress:
            	break;
        }
    }
#endif

    if( !*targetSurfaceCtx && (m_commonSurfaces.size() < MAX_SURFACE_COUNT) && createSurface() )
        *targetSurfaceCtx = m_commonSurfaces.back().data();
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

void QnXVBADecoder::fillOutputFrame( CLVideoDecoderOutput* const outFrame, XVBASurfaceContext* const decodedPicSurface )
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

//		XVBA_Get_Surface_Input getIn;
//		memset( &getIn, 0, sizeof(getIn) );
//		getIn.size = sizeof( getIn );
//		getIn.session = m_decodeSession;
//		getIn.src_surface = decodedPicSurface->surface;
//		getIn.target_buffer = m_decodedPictureRgbaBuf;
//		getIn.target_width = m_sps.pic_width_in_mbs*16;
//		getIn.target_height = m_sps.pic_height_in_map_units*16;
//		getIn.target_pitch = getIn.target_width / 2;
//		getIn.target_parameter.size = sizeof(getIn.target_parameter);
//		getIn.target_parameter.surfaceType = XVBA_YV12;
//		getIn.target_parameter.flag = XVBA_FRAME;
//		m_prevOperationStatus = XVBAGetSurface( &getIn );
//        if( m_prevOperationStatus != Success )
//        {
//            cl_log.log( QString::fromAscii("QnXVBADecoder. Error reading surface. %1").arg(lastErrorText()), cl_logERROR );
//        }

//	    memset( m_decodedPictureRgbaBuf, 0x0, m_decodedPictureRgbaBufSize );
		glBindTexture( GL_TEXTURE_2D, decodedPicSurface->glTexture );
        glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, m_decodedPictureRgbaBuf );
		GLenum glErrorCode = glGetError();
		if( glErrorCode )
			std::cerr<<"Error ("<<glErrorCode<<") reading tex image\n";
        //we have RGBA32 in m_decodedPictureRgbaBuf, but need ARGB32 for QImage...
        for( quint32* curPixel = (quint32*)m_decodedPictureRgbaBuf;
             curPixel < (quint32*)(m_decodedPictureRgbaBuf + m_sps.pic_width_in_mbs*16*m_sps.pic_height_in_map_units*16*4);
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
			m_sps.pic_height_in_map_units*16,
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
    outFrame->height = m_sps.pic_height_in_map_units*16;

#ifdef USE_OPENGL_SURFACE
    Q_ASSERT( !decodedPicSurface || decodedPicSurface->state <= XVBASurfaceContext::renderingInProgress );
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
//        outFrame->reallocate( m_sps.pic_width_in_mbs*16, m_sps.pic_height_in_map_units*16, PIX_FMT_NV12 );
//        memcpy( outFrame->data[0], decodedFrame->Data.Y, decodedFrame->Data.Pitch * m_sps.pic_height_in_map_units*16 );
//        memcpy( outFrame->data[1], decodedFrame->Data.U, decodedFrame->Data.Pitch / 2 * m_sps.pic_height_in_map_units*16 / 2 );
//        memcpy( outFrame->data[2], decodedFrame->Data.V, decodedFrame->Data.Pitch / 2 * m_sps.pic_height_in_map_units*16 / 2 );
//    }
//    else
//    {
//        outFrame->data[0] = decodedFrame->Data.Y;
//        outFrame->data[1] = decodedFrame->Data.U;
//        outFrame->data[2] = decodedFrame->Data.V;
//    }
//    if( pixelFormat == PIX_FMT_NV12 )
//    {
//        outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
//        //outFrame->linesize[1] = decodedFrame->Data.Pitch;   //uv_stride
//        //outFrame->linesize[2] = 0;
//        outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //uv_stride
//        outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;
//    }
#endif

#ifdef USE_OPENGL_SURFACE
    outFrame->format = PIX_FMT_RGBA;
#else
    outFrame->format = PIX_FMT_NV12;
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
    static const size_t NEW_BUFFER_COUNT = 4;

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
    in.num_of_buffers = NEW_BUFFER_COUNT;

	XVBA_Create_DecodeBuff_Output out;
	memset( &out, 0, sizeof(out) );
	out.size = sizeof(out);
    out.num_of_buffers_in_list = NEW_BUFFER_COUNT;
    XVBABufferDescriptor xvbaBufferArray[NEW_BUFFER_COUNT];
	out.buffer_list = xvbaBufferArray;

	m_prevOperationStatus = XVBACreateDecodeBuffers( &in, &out );
    if( (m_prevOperationStatus != Success) || (out.num_of_buffers_in_list == 0) )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error creating buffer of type %1: %2").arg(bufferType).arg(lastErrorText()), cl_logERROR );
        return NULL;
    }

    for( size_t i = 0; i < out.num_of_buffers_in_list; ++i )
    {
        if( out.buffer_list[i].buffer_type == XVBA_PICTURE_DESCRIPTION_BUFFER )
        {
            if( out.buffer_list[i].buffer_size < sizeof(m_pictureDescriptor) )
            {
                cl_log.log( QString::fromAscii("QnXVBADecoder. Error creating buffer of type %1: Created buffer has size %2 while expected %3").
                    arg(bufferType).arg(out.buffer_list[i].buffer_size).arg(sizeof(m_pictureDescriptor)), cl_logERROR );
                return NULL;
            }
    //    	memcpy( out.buffer_list[i].bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
    //    	out.buffer_list[i].data_size_in_buffer = sizeof(m_pictureglXMakeCurrentDescriptor);
        }
        else if( out.buffer_list[i].buffer_type == XVBA_DATA_CTRL_BUFFER )
        {
            out.buffer_list[i].data_size_in_buffer = sizeof(XVBADataCtrl);
        }

        out.buffer_list[i].data_offset = 0;

        cl_log.log( QString::fromAscii("QnXVBADecoder. Allocated xvba decode buffer of type %1, size %2").
            arg(bufferType).arg(out.buffer_list[i].buffer_size), cl_logDEBUG1 );

        m_xvbaDecodeBuffers.push_back( make_pair( new XVBABufferDescriptor(out.buffer_list[i]), false ) );  //TODO get rid of new
    }

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

    m_glSurfaces.clear();
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
    Q_ASSERT( decode_picture_input->buffer_list[0]->buffer_type == XVBA_DATA_BUFFER );
    for( size_t i = 1; i < decode_picture_input->num_of_buffers_in_list; ++i )
    {
        Q_ASSERT( decode_picture_input->buffer_list[i]->buffer_type == XVBA_DATA_CTRL_BUFFER );
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
