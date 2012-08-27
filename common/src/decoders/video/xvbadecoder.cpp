////////////////////////////////////////////////////////////
// 15 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoder.h"

#include <unistd.h>

#include <iostream>

#include <QX11Info>


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

QnXVBADecoder::GLSurfaceContext::GLSurfaceContext( unsigned int _glTexture, XVBASurface _surface )
:
    glTexture( _glTexture ),
    surface( _surface )
{
}


/////////////////////////////////////////////////////
//  class QnXVBADecoder::QnXVBAOpenGLPictureData
/////////////////////////////////////////////////////
QnXVBADecoder::QnXVBAOpenGLPictureData::QnXVBAOpenGLPictureData( GLSurfaceContext* ctx )
:
    m_ctx( ctx )
{
    Q_ASSERT( m_ctx->state == GLSurfaceContext::readyToRender );
    m_ctx->state = GLSurfaceContext::renderingInProcess;
}

QnXVBADecoder::QnXVBAOpenGLPictureData::~QnXVBAOpenGLPictureData()
{
    m_ctx->state = GLSurfaceContext::ready;
}

//!Returns OGL texture name
unsigned int QnXVBADecoder::QnXVBAOpenGLPictureData::glTexture() const
{
    return m_ctx->glTexture;
}

//!Returns context of texture
GLXContext QnXVBADecoder::QnXVBAOpenGLPictureData::glContext() const
{
    //TODO/IMPL
    return NULL;
}


/////////////////////////////////////////////////////
//  class QnSysMemPictureData
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
//  class QnOpenGLPictureData
/////////////////////////////////////////////////////
//!Returns OGL texture name
unsigned int QnOpenGLPictureData::glTexture() const
{
    //TODO/IMPL
    return 0;
}

//!Returns context of texture
GLXContext QnOpenGLPictureData::glContext() const
{
    //TODO/IMPL
    return NULL;
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
    m_getCapDecodeOutputSize( 0 )
{
    cl_log.log( QString::fromAscii("Initializing XVBA decoder"), cl_logDEBUG1 );

    int version = 0;
    m_display = QX11Info::display();    //retreiving display pointer
    m_hardwareAccelerationEnabled = XVBAQueryExtension( m_display, &version );
    if( !m_hardwareAccelerationEnabled )
    {
        std::cout<<"mark10\n";
        cl_log.log( QString::fromAscii("XVBA decode acceleration is not supported on host system"), cl_logERROR );
        return;
    }
    cl_log.log( QString::fromAscii("XVBA of version %1 found").arg(version), cl_logINFO );

    if( !createContext() )
    {
        std::cout<<"mark11\n";
        m_hardwareAccelerationEnabled = false;
        return;
    }

    if( !readSequenceHeader(data) || !checkDecodeCapabilities() )
    {
        std::cout<<"mark12\n";
        m_hardwareAccelerationEnabled = false;
        return;
    }

        std::cout<<"mark13\n";
    
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
}

QnXVBADecoder::~QnXVBADecoder()
{
    if( m_decodeSession )
    {
        XVBADestroyDecode( m_decodeSession );
        m_decodeSession = NULL;
    }

    //destroy surfaces
    for( std::vector<GLSurfaceContext>::size_type
         i = 0;
         i < m_glSurfaces.size();
         ++i )
    {
        XVBADestroySurface( m_glSurfaces[i].surface );
        //releasing gltexture
        glDeleteTextures( 1, &m_glSurfaces[i].glTexture );
    }

    if( m_context )
    {
        XVBADestroyContext( m_context );
        m_context = NULL;
    }

    if( m_glContext )
        glXDestroyContext( m_display, m_glContext );
}

//!Implementation of AbstractDecoder::GetPixelFormat
PixelFormat QnXVBADecoder::GetPixelFormat()
{
    return PIX_FMT_NV12;
}

//!Implementation of AbstractDecoder::decode
bool QnXVBADecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{    
    //NOTE decoder may use more than one surface at a time. We must not block until picture decoding is finished,
        //since decoder may require some more data to finish picture decoding
        //We can just remember, that this frame with pts is being decoded to this surface

    Q_ASSERT( m_hardwareAccelerationEnabled );
    Q_ASSERT( m_context );

    std::cout<<"mark31\n";
    
    if( !m_decodeSession && !createDecodeSession( data ) )
        return false;

    std::cout<<"mark32\n";

    //checking, if one of surfaces is done already
    GLSurfaceContext* targetSurfaceCtx = NULL;
    GLSurfaceContext* decodedPicSurface = NULL;
    checkSurfaces( &targetSurfaceCtx, &decodedPicSurface );

    if( decodedPicSurface )
    {
        //TODO/IMPL copying picture to output
        //*outFrame = *decodedPicSurface;
        decodedPicSurface->state = GLSurfaceContext::renderingInProcess;
    }

    if( !targetSurfaceCtx )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not find unused gl surface. Total surface number: %1").arg(m_glSurfaces.size()), cl_logWARNING );
        return decodedPicSurface != NULL;
    }
    Q_ASSERT( targetSurfaceCtx->state == GLSurfaceContext::ready );
    m_decodeStartInput.target_surface = &targetSurfaceCtx->surface;
    m_prevOperationStatus = XVBAStartDecodePicture( &m_decodeStartInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Could not start picture decoding. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    //TODO/IMPL in case of interlaced video, possibly we'll have to split incoming frame to two fields

    size_t firstSliceOffset = 0;
    analyzeInputBufSlices( data, &m_pictureDescriptor, &m_dataCtrlBuffers, &firstSliceOffset );

    //giving XVBA_PICTURE_DESCRIPTOR_BUFFER
    m_prevOperationStatus = XVBADecodePicture( &m_pictureDescriptorDecodeInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving XVBA_PICTURE_DESCRIPTOR_BUFFER to decoder. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    //giving XVBA_DATA_BUFFER & XVBA_DATA_CTRL_BUFFER
    m_ctrlBufDescriptor.buffer_size = m_dataCtrlBuffers.size() * sizeof(XVBADataCtrl);
    m_ctrlBufDescriptor.bufferXVBA = &m_dataCtrlBuffers[0];
    m_ctrlBufDescriptor.data_size_in_buffer = m_ctrlBufDescriptor.buffer_size;

    //TODO/IMPL align data buf to 128 bytes
    m_dataBufDescriptor.buffer_size = ALIGN128(data->data.size() - firstSliceOffset);
    m_dataBufDescriptor.bufferXVBA = data->data.data() + firstSliceOffset;
    m_dataBufDescriptor.data_size_in_buffer = m_dataBufDescriptor.buffer_size;

    m_prevOperationStatus = XVBADecodePicture( &m_srcDataDecodeInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving src data to decode to decoder. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    m_prevOperationStatus = XVBAEndDecodePicture( &m_decodeEndInput );
    if( m_prevOperationStatus != Success )
    {
        cl_log.log( QString::fromAscii("QnXVBADecoder. Error giving Decode_Picture_End to decoder. %1").arg(lastErrorText()), cl_logERROR );
        return decodedPicSurface != NULL;
    }

    targetSurfaceCtx->pts = data->timestamp;
    targetSurfaceCtx->state = GLSurfaceContext::decodingInProcess;
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
        if( out.decode_caps_list[foundCapIndex].capability_id != XVBA_H264 )
            continue;

        if( (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_BASELINE && m_sps.profile_idc == H264Profile::baseline) ||
            (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_MAIN && m_sps.profile_idc == H264Profile::main) ||
            (out.decode_caps_list[foundCapIndex].flags == XVBA_H264_HIGH && m_sps.profile_idc >= H264Profile::high) )
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

bool QnXVBADecoder::createDecodeSession( const QnCompressedVideoDataPtr& data )
{
    std::cout<<"mark41\n";

    //===XVBACreateDecode===
    XVBA_Create_Decode_Session_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    if( m_outPicSize.isEmpty() )
    {
        in.width = data->width;
        in.height = data->height;
    }
    else
    {
        in.width = m_outPicSize.width();
        in.height = m_outPicSize.height();
    }
    in.context = m_context;


    m_decodeCap.size            = sizeof m_decodeCap;
    m_decodeCap.capability_id   = XVBA_H264;
    m_decodeCap.flags           = XVBA_H264_HIGH;
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
        std::cout<<"mark42. "<<lastErrorText().toStdString()<<"\n";
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA decode session. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

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
    memset( &m_ctrlBufDescriptor, 0, sizeof(m_ctrlBufDescriptor) );
    m_ctrlBufDescriptor.size = sizeof(m_ctrlBufDescriptor);
    m_ctrlBufDescriptor.buffer_type = XVBA_DATA_CTRL_BUFFER;
    memset( &m_dataBufDescriptor, 0, sizeof(m_dataBufDescriptor) );
    m_dataBufDescriptor.size = sizeof(m_dataBufDescriptor);
    m_dataBufDescriptor.buffer_type = XVBA_DATA_BUFFER;

    m_srcDataDecodeBufArray[0] = &m_ctrlBufDescriptor;
    m_srcDataDecodeBufArray[1] = &m_dataBufDescriptor;

    memset( &m_srcDataDecodeInput, 0, sizeof(m_srcDataDecodeInput) );
    m_srcDataDecodeInput.size = sizeof( m_srcDataDecodeInput );
    m_srcDataDecodeInput.session = m_decodeSession;
    m_srcDataDecodeInput.num_of_buffers_in_list = 2;
    m_srcDataDecodeInput.buffer_list = m_srcDataDecodeBufArray;

        //preparing picture_decode_end
    memset( &m_decodeEndInput, 0, sizeof(m_decodeEndInput) );
    m_decodeEndInput.size = sizeof(m_decodeEndInput);
    m_decodeEndInput.session = m_decodeSession;

    //===XVBACreateDecodeBuffers===
//    XVBA_Create_DecodeBuff_Input in;
//    memset( &in, 0, sizeof(in) );
//    in.size = sizeof(in);
//    in.session =  m_decodeSession;

//    XVBA_Create_DecodeBuff_Output out;
//    memset( &out, 0, sizeof(out) );
//    out.size = sizeof(out);

//    m_prevOperationStatus = XVBACreateDecodeBuffers( &in, &out );
//    if( m_prevOperationStatus != Success )
//        return false;
    
    if( !glXMakeCurrent(
            m_display,
            QX11Info::appRootWindow(QX11Info::appScreen()),
            m_glContext ) )
    {
        std::cerr<<"mark43\n";
        GLenum errorCode = glGetError();
        m_prevOperationStatus = BadDrawable;
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to make opengl context current. %u").arg(errorCode), cl_logERROR );
        return false;
    }

        std::cerr<<"mark45\n";
    
    return m_prevOperationStatus == Success;
}

bool QnXVBADecoder::createSurface()
{
    XVBA_Create_GLShared_Surface_Input in;
    memset( &in, 0, sizeof(in) );
    in.size = sizeof(in);
    in.session = m_decodeSession;
    in.glcontext = NULL;   //TODO get gl context used in application
    //creating gl texture
    GLuint gltexture = 0;
    glGenTextures( 1, &gltexture );
    if( gltexture == 0 )
    {
        GLenum errorCode = glGetError();
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create gltexture. %u").arg(errorCode), cl_logERROR );
        return false;
    }
    glBindTexture( GL_TEXTURE_2D, gltexture );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, m_sps.pic_width_in_mbs, m_sps.pic_height_in_map_units, 0, GL_RGB, GL_INT, NULL );
    in.gltexture = gltexture;

    XVBA_Create_GLShared_Surface_Output out;
    memset( &out, 0, sizeof(out) );
    out.size = sizeof(out);

    m_prevOperationStatus = XVBACreateGLSharedSurface( &in, &out );
    if( m_prevOperationStatus != Success )
    {
        std::cout<<"XVBACreateGLSharedSurface failed "<<lastErrorText().toStdString()<<"\n";
        cl_log.log( QString::fromAscii("QnXVBADecoder. Failed to create XVBA OpenGL surface. %1 (%2)").arg(lastErrorText()).arg(m_prevOperationStatus), cl_logERROR );
        return false;
    }

    std::cout<<"Surface created successfully\n";
    m_glSurfaces.push_back( GLSurfaceContext( in.gltexture, out.surface ) );
    return true;
}

bool QnXVBADecoder::readSequenceHeader( const QnCompressedVideoDataPtr& data )
{
    if( data->data.size() <= 4 )
        return false;

    memset( &m_pictureDescriptor, 0, sizeof(m_pictureDescriptor) );

    std::cout<<"Parsing seq header "<<data->data.size()<<"\n";
    
    bool spsFound = false;
    bool ppsFound = false;
    const quint8* dataEnd = reinterpret_cast<const quint8*>(data->data.data()) + data->data.size();
    for( const quint8
         *curNalu = NALUnit::findNextNAL( reinterpret_cast<const quint8*>(data->data.data()), dataEnd ),
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
                std::cout<<"Parsing sps\n";
                m_sps.decodeBuffer( curNalu, nextNalu );
                m_sps.deserialize();
                spsFound = true;

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
                break;
            }

            case nuPPS:
            {
                std::cout<<"Parsing pps\n";
                m_pps.decodeBuffer( curNalu, nextNalu );
                m_pps.deserialize();
                ppsFound = true;

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
                break;
            }

            default:
                break;
        }
    }

    return spsFound && ppsFound;
}

void QnXVBADecoder::analyzeInputBufSlices(
    const QnCompressedVideoDataPtr& data,
    XVBAPictureDescriptor* const pictureDescriptor,
    std::vector<XVBADataCtrl>* const dataCtrlBuffers,
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
        nextNalu = NALUnit::findNextNAL( curNalu, dataEnd );
        Q_ASSERT( nextNalu > curNalu );
        const quint8* curNaluEnd = nextNalu == dataEnd ? dataEnd : nextNalu - 3;    //3 - length of start_code_prefix
        //TODO/IMPL calculate exact NALU end, since there could be zero bytes after NALU
        switch( *curNalu & 0x1f )
        {
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
                pictureDescriptor->avc_curr_field_order_cnt_list[0] = 0;    //TODO/IMPL TopFieldOrderCnt
                pictureDescriptor->avc_curr_field_order_cnt_list[1] = 0;    //TODO/IMPL BottomFieldOrderCnt
                pictureDescriptor->avc_intra_flag = slice.slice_type == SliceUnit::I_TYPE || slice.slice_type == 7;    //I - frame. Supposing, that only I frames are intra coded
                pictureDescriptor->avc_reference = slice.nal_ref_idc > 0 ? 1 : 0;
            }

            case nuSliceB:
            case nuSliceC:
            case nuSliceWithoutPartitioning:
            {
                if( *firstSliceOffset == 0 )
                    *firstSliceOffset = curNalu - reinterpret_cast<const quint8*>(data->data.data());

                dataCtrlBuffers->push_back( XVBADataCtrl() );
                XVBADataCtrl& dataCtrl = dataCtrlBuffers->back();
                memset( &dataCtrl, 0, sizeof(dataCtrl) );
                dataCtrl.SliceDataLocation = curNalu - reinterpret_cast<const quint8*>(data->data.data());    //TODO offset to slice() or to slice_data()?
                dataCtrl.SliceBytesInBuffer = (curNaluEnd - curNalu) - 3;
                dataCtrl.SliceBitsInBuffer = dataCtrl.SliceBytesInBuffer << 3;
                break;
            }

            default:
                continue;
        }
    }
}

void QnXVBADecoder::checkSurfaces( GLSurfaceContext** const targetSurfaceCtx, GLSurfaceContext** const decodedPicSurface )
{
    for( std::vector<GLSurfaceContext>::size_type
         i = 0;
         i < m_glSurfaces.size();
          )
    {
        switch( m_glSurfaces[i].state )
        {
            case GLSurfaceContext::ready:
                if( !*targetSurfaceCtx )
                    *targetSurfaceCtx = &m_glSurfaces[i];
                break;

            case GLSurfaceContext::decodingInProcess:
                memset( &m_syncIn, 0, sizeof(m_syncIn) );
                m_syncIn.size = sizeof(m_syncIn);
                m_syncIn.session = m_decodeSession;
                m_syncIn.surface = &m_glSurfaces[i].surface;
                m_syncIn.query_status = (XVBA_QUERY_STATUS)(XVBA_GET_SURFACE_STATUS | XVBA_GET_DECODE_ERRORS);
                memset( &m_syncOut, 0, sizeof(m_syncOut) );
                m_syncOut.size = sizeof(m_syncOut);
                m_syncOut.decode_error.size = sizeof(m_syncOut.decode_error.size);
                m_prevOperationStatus = XVBASyncSurface( &m_syncIn, &m_syncOut );
                if( m_prevOperationStatus != Success )
                {
                    cl_log.log( QString::fromAscii("QnXVBADecoder. Error synchronizing decoding. %1").arg(lastErrorText()), cl_logERROR );
                    break;
                }
                if( m_syncOut.status_flags & XVBA_STILL_PENDING )
                    break;   //surface still being used
                if( (m_syncOut.status_flags & XVBA_COMPLETED) && (m_syncOut.status_flags & XVBA_NO_ERROR_DECODE) )
                {
                    m_glSurfaces[i].state = GLSurfaceContext::readyToRender;
                    continue;   //checking surface once again (in case it is the only surface with decoded picture)
                }
                if( (m_syncOut.status_flags & XVBA_COMPLETED) && (m_syncOut.status_flags & XVBA_ERROR_DECODE) )
                {
                    cl_log.log( QString::fromAscii("QnXVBADecoder. Decode error occured: %1, %2 macroblocks were not decoded").
                        arg(decodeErrorToString(m_syncOut.decode_error.type)).arg(m_syncOut.decode_error.num_of_bad_mbs), cl_logERROR );
                }
                break;

            case GLSurfaceContext::readyToRender:
                //can return only one decoded frame at a time. Choosing frame with lowest pts (with respect to overflow)
                //TODO/IMPL check pts for overflow
                if( !*decodedPicSurface || m_glSurfaces[i].pts < (*decodedPicSurface)->pts )
                    *decodedPicSurface = &m_glSurfaces[i];
                break;

            case GLSurfaceContext::renderingInProcess:
                //TODO/IMPL checking whether surface still being used
                if( false /*not used by renderer*/ )
                {
                    m_glSurfaces[i].state = GLSurfaceContext::ready;
                    continue;   //checking surface once again (in case it is the only ready surface)
                }
                break;
        }

        ++i;
    }

    if( !*targetSurfaceCtx && (m_glSurfaces.size() < MAX_SURFACE_COUNT) && createSurface() )
        *targetSurfaceCtx = &m_glSurfaces.back();
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
