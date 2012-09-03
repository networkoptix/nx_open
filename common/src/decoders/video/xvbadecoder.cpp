////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoder.h"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>

#include <QX11Info>

#define USE_OPENGL_SURFACE
#define SAVE_DECODED_FRAME_TO_FILE
#define DUMP_INPUT_STREAM
#define DUMP_DECODER_INPUT
#ifdef DUMP_DECODER_INPUT
//#define DUMP_BY_SLICES
#endif


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
//  class QnXVBADecoder::GLSurfaceContext
/////////////////////////////////////////////////////
QnXVBADecoder::GLSurfaceContext::GLSurfaceContext()
:
    glTexture( 0 ),
    surface( NULL ),
    state( ready ),
    pts( 0 )
{
}

QnXVBADecoder::GLSurfaceContext::GLSurfaceContext(
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

QnXVBADecoder::GLSurfaceContext::~GLSurfaceContext()
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


/////////////////////////////////////////////////////
//  class QnXVBADecoder::QnXVBAOpenGLPictureData
/////////////////////////////////////////////////////
QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData( GLSurfaceContext* ctx )
:
	QnOpenGLPictureData(
//		ctx->glContext,
		ctx->glTexture ),
    m_ctx( ctx )
{
    Q_ASSERT_X(
    		ctx->state == GLSurfaceContext::readyToRender,
    		"QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData",
    		QString::fromAscii("ctx->state = %1").arg(ctx->state).toAscii().data() );
    m_ctx->state = GLSurfaceContext::renderingInProcess;
}

QnXVBADecoder::QnXVBAOpenGLPictureData::~QnXVBAOpenGLPictureData()
{
    m_ctx->state = GLSurfaceContext::ready;
}


/////////////////////////////////////////////////////
//  class QnXVBADecoder
/////////////////////////////////////////////////////
QnXVBADecoder::QnXVBADecoder( const QnCompressedVideoDataPtr& data )
:
    m_prevOperationStatus( Success ),
    m_context( NULL ),
    m_decodeSession( NULL ),
    m_glContext( NULL ),
    m_display( NULL ),
    m_getCapDecodeOutputSize( 0 ),
    m_mediaBuf( NULL ),
    m_mediaBufCapacity( 0 ),
    m_decodedPictureRgbaBuf( NULL ),
    m_decodedPictureRgbaBufSize( 0 )
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

#ifdef USE_OPENGL_SURFACE
    //TODO/IMPL creating OGL context shared with drawing context
    static int visualAttribList[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 4,
        GLX_GREEN_SIZE, 4,
        GLX_BLUE_SIZE, 4,
        None };
    XVisualInfo* visualInfo = glXChooseVisual( m_display, QX11Info::appScreen(), visualAttribList );
    m_glContext = glXCreateContext( m_display, visualInfo, NULL, true );
    if( !m_glContext )
    {
        XVBADestroyContext( m_context );
        m_context = NULL;
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create OpenGL context"), cl_logERROR );
    }
    else
    {
        //TODO make context current? (assuming we have one QnXVBADecoder object per thread)
    }
#endif

    m_mediaBufCapacity = 1024*1024;
    m_mediaBuf = new quint8[m_mediaBufCapacity];

#ifdef DUMP_DECODER_INPUT
    m_inStream.open( "/home/ak/slices/total.264", std::ios_base::binary );
#endif
}

QnXVBADecoder::~QnXVBADecoder()
{
	delete[] m_mediaBuf;

    if( m_decodeSession )
    {
        XVBADestroyDecode( m_decodeSession );
        m_decodeSession = NULL;
    }

    m_glSurfaces.clear();

    if( m_context )
    {
        XVBADestroyContext( m_context );
        m_context = NULL;
    }

#ifdef USE_OPENGL_SURFACE
    if( m_glContext )
        glXDestroyContext( m_display, m_glContext );
#endif

    //TODO/IMPL delete decode buffers

    delete[] m_decodedPictureRgbaBuf;
    m_decodedPictureRgbaBuf = NULL;
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
    //NOTE decoder may use more than one surface at a time. We must not block until picture decoding is finished,
        //since decoder may require some more data to finish picture decoding
        //We can just remember, that this frame with pts is being decoded to this surface

    Q_ASSERT( m_hardwareAccelerationEnabled );
    Q_ASSERT( m_context );

    if( !m_decodeSession && !createDecodeSession() )
        return false;

    //checking, if one of surfaces is done already
    GLSurfaceContext* targetSurfaceCtx = NULL;
    GLSurfaceContext* decodedPicSurface = NULL;
    checkSurfaces( &targetSurfaceCtx, &decodedPicSurface );
    Q_ASSERT( !decodedPicSurface || decodedPicSurface->state <= GLSurfaceContext::renderingInProcess );

    if( decodedPicSurface )
    {
        //TODO/IMPL copying picture to output
        cl_log.log( QString::fromAscii("QnXVBADecoder. Found decoded picture (pts %1). Providing it to output...").arg(decodedPicSurface->pts), cl_logDEBUG1 );
    	fillOutputFrame( outFrame, decodedPicSurface );
        decodedPicSurface->state = GLSurfaceContext::renderingInProcess;
    }

    if( !targetSurfaceCtx )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find unused gl surface. Total surface number: %1").arg(m_glSurfaces.size()), cl_logWARNING );
        return decodedPicSurface != NULL;
    }

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    Q_ASSERT( targetSurfaceCtx->state == GLSurfaceContext::ready );
    m_decodeStartInput.target_surface = targetSurfaceCtx->surface;
    m_prevOperationStatus = XVBAStartDecodePicture( &m_decodeStartInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not start picture decoding. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    //TODO/IMPL in case of interlaced video, possibly we'll have to split incoming frame to two fields

//    XVBABufferDescriptor* dataCtrlBufferDescriptor = getDecodeBuffer( XVBA_DATA_CTRL_BUFFER );
//    if( !dataCtrlBufferDescriptor )
//    {
//        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_CTRL_BUFFER buffer"), cl_logERROR );
//        return decodedPicSurface != NULL;
//    }
    XVBABufferDescriptor* pictureDescriptionBufferDescriptor = getDecodeBuffer( XVBA_PICTURE_DESCRIPTION_BUFFER );
    if( !pictureDescriptionBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_PICTURE_DESCRIPTION_BUFFER buffer"), cl_logERROR );
//        ungetBuffer( dataCtrlBufferDescriptor );
        return decodedPicSurface != NULL;
    }
	memcpy( pictureDescriptionBufferDescriptor->bufferXVBA, &m_pictureDescriptor, sizeof(m_pictureDescriptor) );
	pictureDescriptionBufferDescriptor->data_size_in_buffer = sizeof(m_pictureDescriptor);

	XVBABufferDescriptor* dataBufferDescriptor = getDecodeBuffer( XVBA_DATA_BUFFER );
    if( !dataBufferDescriptor )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. No XVBA_DATA_BUFFER buffer"), cl_logERROR );
        ungetBuffer( pictureDescriptionBufferDescriptor );
        return decodedPicSurface != NULL;
    }

    targetSurfaceCtx->usedDecodeBuffers.push_back( pictureDescriptionBufferDescriptor );
    targetSurfaceCtx->usedDecodeBuffers.push_back( dataBufferDescriptor );
    size_t firstSliceOffset = 0;
    if( !analyzeInputBufSlices(
    		data,
    		static_cast<XVBAPictureDescriptor*>(pictureDescriptionBufferDescriptor->bufferXVBA),
    		&targetSurfaceCtx->usedDecodeBuffers,
    		&firstSliceOffset ) )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find slice in source data"), cl_logWARNING );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        return decodedPicSurface != NULL;
    }

    //giving XVBA_PICTURE_DESCRIPTOR_BUFFER
    m_pictureDescriptorBufArray[0] = pictureDescriptionBufferDescriptor;
    m_prevOperationStatus = XVBADecodePicture( &m_pictureDescriptorDecodeInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving XVBA_PICTURE_DESCRIPTOR_BUFFER to decoder. %1").arg(lastErrorText()), cl_logERROR );
        std::for_each(
        	targetSurfaceCtx->usedDecodeBuffers.begin(),
        	targetSurfaceCtx->usedDecodeBuffers.end(),
        	std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
        return decodedPicSurface != NULL;
    }

    //giving XVBA_DATA_BUFFER & XVBA_DATA_CTRL_BUFFER
//    m_ctrlBufDescriptor.buffer_size = m_dataCtrlBuffers.size() * sizeof(XVBADataCtrl);
//    m_ctrlBufDescriptor.bufferXVBA = &m_dataCtrlBuffers[0];
//    m_ctrlBufDescriptor.data_size_in_buffer = m_ctrlBufDescriptor.buffer_size;

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
//    if( m_mediaBufCapacity < data->data.size() + 128 )
//    {
//    	m_mediaBufCapacity = data->data.size() + 128;
//    	delete[] m_mediaBuf;
//    	m_mediaBuf = new quint8[m_mediaBufCapacity];
//    }
//    char* dataStart = (char*)ALIGN128((size_t)m_mediaBuf);
	char* dataStart = reinterpret_cast<char*>(ALIGN128((size_t)dataBufferDescriptor->bufferXVBA));
	dataBufferDescriptor->data_offset = dataStart - static_cast<char*>(dataBufferDescriptor->bufferXVBA);
    memcpy( dataStart, data->data.data(), data->data.size() );
    dataBufferDescriptor->data_size_in_buffer = ALIGN128(data->data.size());
    memset( dataStart + data->data.size(), 0, dataBufferDescriptor->data_size_in_buffer - data->data.size() );

#ifdef DUMP_DECODER_INPUT
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
#ifdef DUMP_BY_SLICES
    		m_inStream.write(
    			(const char*)dataBufferDescriptor->bufferXVBA + dataBufferDescriptor->data_offset + ((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceDataLocation,
				((XVBADataCtrl*)targetSurfaceCtx->usedDecodeBuffers[i]->bufferXVBA)->SliceBytesInBuffer );
#endif
    	}
#ifndef DUMP_BY_SLICES
    	m_inStream.write( data->data.data(), data->data.size() );
#endif
    }
#endif

//    m_dataBufDescriptor.buffer_size = ALIGN128(data->data.size() /*- firstSliceOffset*/);
//    m_dataBufDescriptor.bufferXVBA = dataStart;//dataStart /*+ firstSliceOffset*/;
//    m_dataBufDescriptor.data_size_in_buffer = data->data.size();

//    m_srcDataDecodeBufArray[0] = dataBufferDescriptor;
//    m_srcDataDecodeBufArray[1] = dataCtrlBufferDescriptor;

    m_srcDataDecodeInput.buffer_list = &targetSurfaceCtx->usedDecodeBuffers[1];	//index 0 - pictureDescriptionBufferDescriptor which is not needed here
    m_srcDataDecodeInput.num_of_buffers_in_list = targetSurfaceCtx->usedDecodeBuffers.size()-1;

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
    targetSurfaceCtx->state = GLSurfaceContext::decodingInProcess;

//    m_syncIn.surface = targetSurfaceCtx->surface;
//    m_syncIn.query_status = XVBA_GET_SURFACE_STATUS;	//XVBA_GET_DECODE_ERRORS not supported
//    memset( &m_syncOut, 0, sizeof(m_syncOut) );
//    m_syncOut.size = sizeof(m_syncOut);
//    m_prevOperationStatus = XVBASyncSurface( &m_syncIn, &m_syncOut );
//    if( m_prevOperationStatus != Success )
//    {
//        cl_log.log( QString::fromAscii("QnXVBADecoder. HUY"), cl_logERROR );
//    }
//    cl_log.log( QString::fromAscii("QnXVBADecoder. m_syncOut.status_flags = %1").arg(m_syncOut.status_flags), cl_logDEBUG1 );

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

void QnXVBADecoder::setLightCpuMode( QnAbstractVideoDecoder::DecodeMode )
{
    //TODO/IMPL
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
    m_pictureDescriptorDecodeInput.num_of_buffers_in_list = 1;
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

    m_decodedPictureRgbaBufSize = in.width * in.height * 4;
    m_decodedPictureRgbaBuf = new quint8[m_decodedPictureRgbaBufSize];
    memset( m_decodedPictureRgbaBuf, 0xf0, m_decodedPictureRgbaBufSize );
    for( size_t i = 0; i < m_decodedPictureRgbaBufSize; i+=2 )
    {
    	m_decodedPictureRgbaBuf[i] = 0xf8;
    	m_decodedPictureRgbaBuf[i+1] = 0x30;
    }

#ifdef USE_OPENGL_SURFACE
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
#endif

    return m_prevOperationStatus == Success;
}

bool QnXVBADecoder::createSurface()
{
#ifdef USE_OPENGL_SURFACE
    //creating gl texture
//    if( !glXMakeCurrent(
//            m_display,
//            QX11Info::appRootWindow(QX11Info::appScreen()),
//            m_glContext ) )
//    {
//        GLenum errorCode = glGetError();
//        m_prevOperationStatus = BadDrawable;
//        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to make opengl context current. %u").arg(errorCode), cl_logERROR );
//        return false;
//    }

    GLuint gltexture = 0;
    glGenTextures( 1, &gltexture );
    GLenum glErrorCode = 0;
    if( gltexture == 0 )
    {
        glErrorCode = glGetError();
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create gltexture. %1").arg(glErrorCode), cl_logERROR );
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
                  GL_RGBA,
                  GL_UNSIGNED_SHORT_4_4_4_4,
                  m_decodedPictureRgbaBuf );
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
        std::cout<<"XVBACreateGLSharedSurface failed "<<lastErrorText().toStdString()<<"\n";
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA OpenGL surface. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        glDeleteTextures( 1, &gltexture );
        return false;
    }

    m_glSurfaces.push_back( QSharedPointer<GLSurfaceContext>( new GLSurfaceContext( m_glContext, in.gltexture, out.surface ) ) );
    cl_log.log( QString::fromAscii("XVBA opengl surface created successfully. Total surface count: %1" ).arg(m_glSurfaces.size()), cl_logDEBUG1 );

//    if( !glXMakeCurrent(
//            m_display,
//            None,
//            NULL ) )
//    {
//        GLenum errorCode = glGetError();
//        m_prevOperationStatus = BadDrawable;
//        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to reset current opengl context. %u").arg(errorCode), cl_logERROR );
//        return false;
//    }

    return true;
#else
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
        std::cout<<"XVBACreateSurface failed "<<lastErrorText().toStdString()<<"\n";
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA surface. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

    cl_log.log( QString::fromAscii("XVBA surface created successfully" ), cl_logDEBUG1 );
    m_glSurfaces.push_back( QSharedPointer<GLSurfaceContext>( new GLSurfaceContext( NULL, 0, out.surface ) ) );
    return true;
#endif
}

bool QnXVBADecoder::readSequenceHeader( const QnCompressedVideoDataPtr& data )
{
#ifndef DUMP_BY_SLICES
	m_inStream.write( data->data.data(), data->data.size() );
#endif

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
        switch( *curNalu & 0x1f )
        {
            case nuSPS:
            {
            	readSPS( curNalu, nextNalu );
            	spsFound = true;
                break;
            }

            case nuPPS:
            {
            	readPPS( curNalu, nextNalu );
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
    std::vector<XVBABufferDescriptor*>* const dataCtrlBuffers,
    size_t* firstSliceOffset )
{
	*firstSliceOffset = 0;

    //searching for a first slice
    const quint8* dataEnd = reinterpret_cast<const quint8*>(data->data.data()) + data->data.size();
    for( const quint8
         *curNalu = NALUnit::findNextNAL( reinterpret_cast<const quint8*>(data->data.data()), dataEnd ),
         *nextNalu = NULL;
         curNalu < dataEnd;
         curNalu = nextNalu )
    {
    	const quint8* curNaluWithPrefix = curNalu-4;
        nextNalu = NALUnit::findNextNAL( curNalu, dataEnd );
        Q_ASSERT( nextNalu > curNalu );
        const quint8* curNaluEnd = nextNalu == dataEnd ? dataEnd : (nextNalu - 4);    //4 - length of start_code_prefix
        //TODO/IMPL calculate exact NALU end, since there could be zero bytes after NALU
        switch( *curNalu & 0x1f )
        {
//			case nuSPS:
//			case nuPPS:
//				continue;

			case nuSliceNonIDR:
            case nuSliceA:
            case nuSliceIDR:
            {
                SliceUnit slice;
                slice.decodeBuffer( curNalu, curNaluEnd );
                slice.deserialize( &m_sps, &m_pps );

                //parsing slice_header
                pictureDescriptor->picture_structure = pictureDescriptor->sps_info.avc.frame_mbs_only_flag || (slice.m_field_pic_flag == 0)
                    ? 3  //3 = Frame
                    : slice.bottom_field_flag;  //1 = Bottom field, 0 = Top field
                pictureDescriptor->avc_frame_num = slice.frame_num;
                pictureDescriptor->avc_curr_field_order_cnt_list[0] = slice.frame_num;    //TODO/IMPL TopFieldOrderCnt
                pictureDescriptor->avc_curr_field_order_cnt_list[1] = 0;    //TODO/IMPL BottomFieldOrderCnt
                pictureDescriptor->avc_intra_flag = slice.slice_type == SliceUnit::I_TYPE || slice.slice_type == 7;    //I - frame. Supposing, that only I frames are intra coded
                pictureDescriptor->avc_reference = slice.nal_ref_idc > 0 ? 1 : 0;
            }

			case nuSPS:
			case nuPPS:
            case nuSliceB:
            case nuSliceC:
            case nuSliceWithoutPartitioning:
            {
            	if( (*curNalu & 0x1f) == nuSPS )
    				readSPS( curNalu, curNaluEnd );
            	else if( (*curNalu & 0x1f) == nuPPS )
    				readPPS( curNalu, curNaluEnd );

                if( *firstSliceOffset == 0 )
                    *firstSliceOffset = curNaluWithPrefix - reinterpret_cast<const quint8*>(data->data.data());

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

                dataCtrlBuffers->push_back( dataCtrlBufferDescriptor );
                break;
            }

            default:
                continue;
        }
    }

    return true;
}

void QnXVBADecoder::checkSurfaces( GLSurfaceContext** const targetSurfaceCtx, GLSurfaceContext** const decodedPicSurface )
{
    for( std::list<QSharedPointer<GLSurfaceContext> >::iterator
         it = m_glSurfaces.begin();
         it != m_glSurfaces.end();
          )
    {
        switch( (*it)->state )
        {
            case GLSurfaceContext::ready:
                if( !*targetSurfaceCtx )
                    *targetSurfaceCtx = it->data();
                break;

            case GLSurfaceContext::decodingInProcess:
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
                	//releasing decode buffers
                	std::for_each(
                		(*it)->usedDecodeBuffers.begin(),
                		(*it)->usedDecodeBuffers.end(),
                		std::bind1st( std::mem_fun(&QnXVBADecoder::ungetBuffer), this ) );
                	(*it)->usedDecodeBuffers.clear();

//                	GLSurfaceContext* unusedSurface = findUnusedSurface();
//
//                	XVBA_Transfer_Surface_Input transIn;
//                	memset( &transIn, 0, sizeof(transIn) );
//                	transIn.size = sizeof( transIn );
//                	transIn.session = m_decodeSession;
//                	transIn.src_surface = it->data()->surface;
//                	transIn.target_surface = unusedSurface->surface;
//                	transIn.flag = XVBA_FRAME;
//                	m_prevOperationStatus = XVBATransferSurface( &transIn );
//                    if( m_prevOperationStatus != Success )
//                    {
//                        cl_log.log( QString::fromAscii("QnXVBADecoder. Error transferring surface. %1").arg(lastErrorText()), cl_logERROR );
//                    }
//                    unusedSurface->state = GLSurfaceContext::readyToRender;
//                    (*it)->state = GLSurfaceContext::ready;
                    (*it)->state = GLSurfaceContext::readyToRender;

                	//checking surface for errors...
                	//NOTE check for errors always returns bad param. Assuming everything's fine...
//                    m_syncIn.surface = it->data()->surface;
//                    m_syncIn.query_status = (XVBA_QUERY_STATUS)(XVBA_GET_DECODE_ERRORS);
//                    memset( &m_syncOut, 0, sizeof(m_syncOut) );
//                    m_syncOut.size = sizeof(m_syncOut);
//                    m_syncOut.decode_error.size = sizeof(m_syncOut.decode_error.size);
//                    m_prevOperationStatus = XVBASyncSurface( &m_syncIn, &m_syncOut );
//                    if( m_prevOperationStatus != Success )
//                    {
//                        cl_log.log( QString::fromAscii("QnXVBADecoder. Error reading decode error code. %1").arg(lastErrorText()), cl_logERROR );
//                        //assuming surface is ready
//                        m_glSurfaces[i].state = GLSurfaceContext::ready;
//                        continue;
//                    }
//
//                    if( m_syncOut.status_flags & XVBA_NO_ERROR_DECODE )
//                    {
                        cl_log.log( QString::fromAscii("QnXVBADecoder. Surface is ready to be rendered"), cl_logDEBUG1 );
                        continue;   //checking surface once again (in case it is the only surface with decoded picture)
//                    }
//                    if( m_syncOut.status_flags & XVBA_ERROR_DECODE )
//                    {
//                        cl_log.log( QString::fromAscii("QnXVBADecoder. Decode error occured: %1, %2 macroblocks were not decoded").
//                            arg(decodeErrorToString(m_syncOut.decode_error.type)).arg(m_syncOut.decode_error.num_of_bad_mbs), cl_logERROR );
//                        m_glSurfaces[i].state = GLSurfaceContext::ready;
//                        continue;
//                    }
                }

//                if( (m_syncOut.status_flags & XVBA_COMPLETED) && (m_syncOut.status_flags & XVBA_NO_ERROR_DECODE) )
//                {
//                    cl_log.log( QString::fromAscii("QnXVBADecoder. Surface is ready to be rendered"), cl_logDEBUG1 );
//                    m_glSurfaces[i].state = GLSurfaceContext::readyToRender;
//                    continue;   //checking surface once again (in case it is the only surface with decoded picture)
//                }
//                if( (m_syncOut.status_flags & XVBA_COMPLETED) && (m_syncOut.status_flags & XVBA_ERROR_DECODE) )
//                {
//                    cl_log.log( QString::fromAscii("QnXVBADecoder. Decode error occured: %1, %2 macroblocks were not decoded").
//                        arg(decodeErrorToString(m_syncOut.decode_error.type)).arg(m_syncOut.decode_error.num_of_bad_mbs), cl_logERROR );
//                }
                break;

            case GLSurfaceContext::readyToRender:
                //can return only one decoded frame at a time. Choosing frame with lowest pts (with respect to overflow)
                //TODO/IMPL check pts for overflow
                if( !*decodedPicSurface || (*it)->pts < (*decodedPicSurface)->pts )
                    *decodedPicSurface = it->data();
                break;

            case GLSurfaceContext::renderingInProcess:
            	//surface still being used by renderer
                break;
        }

        ++it;
    }

    if( !*targetSurfaceCtx && (m_glSurfaces.size() < MAX_SURFACE_COUNT) && createSurface() )
        *targetSurfaceCtx = m_glSurfaces.back().data();
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

static int fileNumber = 1;

void QnXVBADecoder::fillOutputFrame( CLVideoDecoderOutput* const outFrame, GLSurfaceContext* const decodedPicSurface )
{
#ifdef SAVE_DECODED_FRAME_TO_FILE
	{
//	    if( !glXMakeCurrent(
//	            m_display,
//	            QX11Info::appRootWindow(QX11Info::appScreen()),
//	            m_glContext ) )
//	    {
//	        GLenum errorCode = glGetError();
//	        std::cerr<<"Failed to make context current. "<<errorCode<<"\n";
//	    }

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
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, m_decodedPictureRgbaBuf );
		GLenum glErrorCode = glGetError();
		if( glErrorCode )
			std::cerr<<"Error ("<<glErrorCode<<") reading tex image\n";
		QImage img(
			m_decodedPictureRgbaBuf,
			m_sps.pic_width_in_mbs*16,
			m_sps.pic_height_in_map_units*16,
			QImage::Format_RGB444 );	//QImage::Format_ARGB4444_Premultiplied );
		const QString& fileName = QString::fromAscii("/home/ak/pic_dump/%1.bmp").arg(fileNumber++);
		if( !img.save( fileName, "bmp" ) )
			std::cerr<<"Failed to save pic to "<<fileName.toStdString()<<"\n";

//	    if( !glXMakeCurrent(
//	            m_display,
//	            None,
//	            NULL ) )
//	    {
//	        GLenum errorCode = glGetError();
//	        std::cerr<<"Failed to reset current context. "<<errorCode<<"\n";
//	    }
	}
#endif

    outFrame->width = m_sps.pic_width_in_mbs*16;
    outFrame->height = m_sps.pic_height_in_map_units*16;

#ifdef USE_OPENGL_SURFACE
    Q_ASSERT( !decodedPicSurface || decodedPicSurface->state <= GLSurfaceContext::renderingInProcess );
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
	vector<pair<XVBABufferDescriptor*, bool> >::size_type unusedBufIndex = 0;
	for( ;
		unusedBufIndex < m_xvbaDecodeBuffers.size();
		++unusedBufIndex )
	{
		if( !m_xvbaDecodeBuffers[unusedBufIndex].second )
			break;
	}
	if( unusedBufIndex < m_xvbaDecodeBuffers.size() )
	{
		m_xvbaDecodeBuffers[unusedBufIndex].second = true;
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
	out.num_of_buffers_in_list = 1;
	XVBABufferDescriptor xvbaBufferArray[1];
	out.buffer_list = xvbaBufferArray;

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
//    	out.buffer_list->data_size_in_buffer = sizeof(m_pictureDescriptor);
    }
    else if( out.buffer_list->buffer_type == XVBA_DATA_CTRL_BUFFER )
    {
    	out.buffer_list->data_size_in_buffer = sizeof(XVBADataCtrl);
    }

    out.buffer_list->data_offset = 0;

    cl_log.log( QString::fromAscii("QnXVBADecoder. Allocated xvba decode buffer of type %1, size %2").
    	arg(bufferType).arg(out.buffer_list->buffer_size), cl_logDEBUG1 );

    m_xvbaDecodeBuffers.push_back( make_pair( out.buffer_list, true ) );
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

	m_pictureDescriptor.profile = m_sps.profile_idc;
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
	m_pictureDescriptor.avc_bit_depth_luma_minus8 = m_sps.bit_depth_luma - 8;
	m_pictureDescriptor.avc_bit_depth_chroma_minus8 = m_sps.bit_depth_chroma - 8;
	m_pictureDescriptor.avc_log2_max_frame_num_minus4 = m_sps.log2_max_frame_num - 4;

	m_pictureDescriptor.avc_pic_order_cnt_type = m_sps.pic_order_cnt_type;
	m_pictureDescriptor.avc_log2_max_pic_order_cnt_lsb_minus4 = m_sps.log2_max_pic_order_cnt_lsb;
	m_pictureDescriptor.avc_num_ref_frames = m_sps.num_ref_frames;
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
	m_pictureDescriptor.avc_second_chroma_qp_index_offset = m_pps.second_chroma_qp_index_offset;

	m_pictureDescriptor.avc_slice_group_change_rate_minus1 = m_pps.slice_group_change_rate - 1;

	m_pictureDescriptor.pps_info.avc.entropy_coding_mode_flag = m_pps.entropy_coding_mode_flag;
	m_pictureDescriptor.pps_info.avc.pic_order_present_flag = m_pps.pic_order_present_flag;
	m_pictureDescriptor.pps_info.avc.weighted_pred_flag = m_pps.weighted_pred_flag;
	m_pictureDescriptor.pps_info.avc.weighted_bipred_idc = m_pps.weighted_bipred_idc;
	m_pictureDescriptor.pps_info.avc.deblocking_filter_control_present_flag = m_pps.deblocking_filter_control_present_flag;
	m_pictureDescriptor.pps_info.avc.constrained_intra_pred_flag = m_pps.constrained_intra_pred_flag;
	m_pictureDescriptor.pps_info.avc.redundant_pic_cnt_present_flag = m_pps.redundant_pic_cnt_present_flag;
	m_pictureDescriptor.pps_info.avc.transform_8x8_mode_flag = m_pps.transform_8x8_mode_flag;
}

QnXVBADecoder::GLSurfaceContext* QnXVBADecoder::findUnusedSurface()
{
    for( std::list<QSharedPointer<GLSurfaceContext> >::iterator
         it = m_glSurfaces.begin();
         it != m_glSurfaces.end();
         ++it )
    {
    	if( (*it)->state == GLSurfaceContext::ready )
    		return it->data();
    }

    createSurface();
    return m_glSurfaces.back().data();
}
