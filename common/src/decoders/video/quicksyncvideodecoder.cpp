/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#include <QDebug>

#ifndef XVBA_TEST
#include "../../utils/media/nalunits.h"
#else
#include "nalunits.h"
#endif

#include "quicksyncvideodecoder.h"


//TODO/IMPL implement shader NV12 -> RGB
//TODO/IMPL make decoder asynchronous
//TODO/IMPL take sequence header from extradata
//TODO/IMPL track usage of decoded plane by renderer

//////////////////////////////////////////////////////////
// QuickSyncVideoDecoder
//////////////////////////////////////////////////////////

//!Maximum tries to decode frame
static const int MAX_ENCODE_ASYNC_CALL_TRIES = 7;
static const int MS_TO_WAIT_FOR_DEVICE = 5;

QuickSyncVideoDecoder::QuickSyncVideoDecoder( const QnCompressedVideoDataPtr data )
:
    m_state( decoding ),
    m_syncPoint( NULL ),
    m_initialized( false ),
    m_mfxSessionEstablished( false ),
    m_decodingInitialized( false ),
    m_totalBytesDropped( 0 ),
    m_prevTimestamp( 0 ),
#ifndef XVBA_TEST
    m_lastAVFrame( NULL ),
#endif
#ifdef USE_D3D
    m_frameAllocator( &m_sysMemFrameAllocator, &m_d3dFrameAllocator ),
#endif
    m_outPictureSize( 0, 0 )
{
    memset( &m_srcStreamParam, 0, sizeof(m_srcStreamParam) );
    memset( &m_decodingAllocResponse, 0, sizeof(m_decodingAllocResponse) );
    memset( &m_vppAllocResponse, 0, sizeof(m_vppAllocResponse) );

#ifdef USE_D3D
    if( !m_d3dFrameAllocator.initialize() )
    {
        cl_log.log( QString::fromAscii("Failed to initialize DXVA frame surface allocator. %1").arg(m_d3dFrameAllocator.getLastErrorText()), cl_logERROR );
        return;
    }
#endif

#ifndef XVBA_TEST
    m_lastAVFrame = avcodec_alloc_frame();
#endif

    //setOutPictureSize( QSize( 320, 180 ) );
    //setOutPictureSize( QSize( 360, 200 ) );
    setOutPictureSize( QSize( 1920/4, 1080/4 ) );

#ifdef WRITE_INPUT_STREAM_TO_FILE
    m_inputStreamFile.open( "C:/temp/in.264", std::ios::binary );
#endif

    //decode( data, NULL );
    //readSequenceHeader( data );

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    inputStream.TimeStamp = data->timestamp;
    inputStream.Data = reinterpret_cast<mfxU8*>(const_cast<char*>(data->data.data()));
    inputStream.DataLength = data->data.size();
    inputStream.MaxLength = data->data.size();

    init( MFX_CODEC_AVC, &inputStream );
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
    m_processor.reset();
    m_decoder.reset();
    m_mfxSession.Close();

    //free surface pool
    if( m_decodingAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_decodingAllocResponse );
    if( m_vppAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_vppAllocResponse );

#ifndef XVBA_TEST
    av_free( m_lastAVFrame );
#endif
}

PixelFormat QuickSyncVideoDecoder::GetPixelFormat() const
{
    return PIX_FMT_NV12;
}

#ifndef XVBA_TEST
static const AVRational SRC_DATA_TIMESTAMP_RESOLUTION = {1, 1000000};
static const AVRational INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION = {1, 90000};
#endif

bool QuickSyncVideoDecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
    //if( !m_initialized )
    //    readSequenceHeader( data );

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    //inputStream.TimeStamp = av_rescale_q( data->timestamp, SRC_DATA_TIMESTAMP_RESOLUTION, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION );
    inputStream.TimeStamp = data->timestamp;
    //inputStream.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    //if( data->context && data->context->ctx() && data->context->ctx()->extradata_size >= 7 && data->context->ctx()->extradata[0] == 1 )
    //{
    //    std::basic_string<mfxU8> seqHeader;
    //    //TODO/IMPL sps & pps is in the extradata, parsing it...
    //    // prefix is unit len
    //    int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;

    //    const mfxU8* curNal = data->context->ctx()->extradata+5;
    //    const mfxU8* dataEnd = data->context->ctx()->extradata + data->context->ctx()->extradata_size;
    //    while( curNal < dataEnd - reqUnitSize )
    //    {
    //        unsigned int curSize = 0;
    //        for( int i = 0; i < reqUnitSize; ++i ) 
    //            curSize = (curSize << 8) + curNal[i];
    //        curNal += reqUnitSize;
    //        curSize = std::min<>(curSize, (unsigned int)(dataEnd - curNal));
    //        seqHeader.append( curNal, curSize );

    //        curNal += curSize;
    //    }

    //    inputStream.Data = const_cast<mfxU8*>(seqHeader.data());
    //    inputStream.DataLength = seqHeader.size();
    //    inputStream.MaxLength = seqHeader.size();
    //    decode( data->compressionType == CODEC_ID_H264 ? MFX_CODEC_AVC : MFX_CODEC_MPEG2, &inputStream, outFrame );
    //}

#ifdef WRITE_INPUT_STREAM_TO_FILE
    m_inputStreamFile.write( data->data.data(), data->data.size() );
    m_inputStreamFile.close();
#endif

    //qDebug()<<"Input timestamp: "<<inputStream.TimeStamp;
    //if( m_prevTimestamp > inputStream.TimeStamp )
    //    qDebug()<<"Warning! timestamp decreased by "<<(m_prevTimestamp - inputStream.TimeStamp);
    //m_prevTimestamp = inputStream.TimeStamp;

#ifndef XVBA_TEST
    inputStream.Data = reinterpret_cast<mfxU8*>(data->data.data());
#else
    inputStream.Data = reinterpret_cast<mfxU8*>(const_cast<char*>(data->data.data()));
#endif
    inputStream.DataLength = data->data.size();
    inputStream.MaxLength = data->data.size();
    //inputStream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    return decode( MFX_CODEC_AVC, &inputStream, outFrame );
}

bool QuickSyncVideoDecoder::decode( mfxU32 codecID, mfxBitstream* inputStream, CLVideoDecoderOutput* outFrame )
{
    cl_log.log( QString("QuickSyncVideoDecoder::decode. data.size = %1").arg(inputStream->DataLength), cl_logDEBUG1 );

    if( !m_initialized && !init( codecID, inputStream ) )
        return false;

    bool gotDisplayPicture = false;

    mfxFrameSurface1* decodedFrame = NULL;
    unsigned int prevInputStreamDataOffset = inputStream->DataOffset;
    for( int decodingTry = 0;
        decodingTry < MAX_ENCODE_ASYNC_CALL_TRIES;
         )
    {
        mfxStatus opStatus = MFX_ERR_NONE;
        m_state = decoding;
        while( (m_state != done) && (opStatus >= MFX_ERR_NONE) )
        {
            switch( m_state )
            {
                case decoding:
                {
                    mfxFrameSurface1* workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
                    opStatus = m_decoder->DecodeFrameAsync(
                        workingSurface ? inputStream : NULL,
                        workingSurface,
                        &decodedFrame,
                        &m_syncPoint );
                    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. starting frame (pts %1) decoding. Result %2").
                        arg(inputStream->TimeStamp).arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
                    break;  
                }

                case processing:
                {
                    mfxFrameSurface1* in = decodedFrame;
                    decodedFrame = findUnlockedSurface( &m_processingSurfacePool );
                    opStatus = m_processor->RunFrameVPPAsync( in, decodedFrame, NULL, &m_syncPoint );
                    m_state = done;
                    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. starting frame (pts %1) processing. Result %2").
                        arg(decodedFrame->Data.TimeStamp).arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
                    break;
                }
            }

            if( opStatus >= MFX_ERR_NONE && m_syncPoint )
            {
                opStatus = MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE );
                cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. sync operation result %1").arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
            }
            else if( opStatus > MFX_ERR_NONE )  //DecodeFrameAsync returned warning
            {
                if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
                {
                    //updating stored video-stream parameters
                    m_decoder->GetVideoParam( &m_srcStreamParam );
                }
                opStatus = MFX_ERR_NONE;
                continue;
            }

            if( opStatus > MFX_ERR_NONE )
                opStatus = MFX_ERR_NONE;   //ignoring warnings

            //moving state
            switch( m_state )
            {
                case decoding:
                    m_state = m_processor.get() ? processing : done;
                    break;

                case processing:
                    m_state = done;
                    break;
            }
        }

        //TODO/IMPL perhaps it is reasonable to do SyncOperation before DecodeFrameAsync (to sync previous operation) to parallel decoding and everything but not this method

        switch( opStatus )
        {
            case MFX_ERR_NONE:
            {
                if( !gotDisplayPicture )
                {
                    cl_log.log( QString::fromAscii("Providing picture at output. Out frame timestamp %1, frame order %2, corrupted %3").
                        arg(decodedFrame->Data.TimeStamp).arg(decodedFrame->Data.FrameOrder).arg(decodedFrame->Data.Corrupted), cl_logDEBUG1 );
                    if( m_prevTimestamp > decodedFrame->Data.TimeStamp )
                    {
                        cl_log.log( QString::fromAscii("Warning! timestamp decreased by %1. Current 90KHz timestamp %2, previous %3. Ignoring frame...").
                            arg(QString::number(m_prevTimestamp - decodedFrame->Data.TimeStamp)).
                            arg(QString::number(decodedFrame->Data.TimeStamp)).
                            arg(QString::number(m_prevTimestamp)),
                            cl_logINFO );
                        m_prevTimestamp = decodedFrame->Data.TimeStamp;
                        //ignoring frame
                        //break;
                    }
                    m_prevTimestamp = decodedFrame->Data.TimeStamp;

                    //outFrame = decodedFrame
                    m_frameAllocator.Lock( &m_frameAllocator, decodedFrame->Data.MemId, &decodedFrame->Data );

                    if( outFrame )
                    {
                        saveToAVFrame( outFrame, decodedFrame );
#ifndef XVBA_TEST
                        *m_lastAVFrame = *outFrame;
                        m_lastAVFrame->data[0] = decodedFrame->Data.Y;
                        m_lastAVFrame->data[1] = decodedFrame->Data.U;
                        m_lastAVFrame->data[2] = decodedFrame->Data.V;
#endif
                    }

                    m_frameAllocator.Unlock( &m_frameAllocator, decodedFrame->Data.MemId, &decodedFrame->Data );
                }
                else
                {
                    //decoder returned second picture for same input. Enqueue?
                    cl_log.log( QString::fromAscii("Warning! Got second picture from Intel media decoder during single decoding step! Ignoring..."), cl_logWARNING );
                }

                gotDisplayPicture = true;
                break;
            }

            case MFX_ERR_MORE_DATA:
                break;

            case MFX_ERR_MORE_SURFACE:
                //provide more surface space and try again...
                continue;

            case MFX_WRN_DEVICE_BUSY:
                Sleep( MS_TO_WAIT_FOR_DEVICE );
                ++decodingTry;
                continue;

            case MFX_ERR_DEVICE_LOST:
            case MFX_ERR_DEVICE_FAILED:
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
                //trying to reset decoder
                m_decoder->Reset( &m_srcStreamParam );
                if( m_processor.get() )
                    m_processor->Reset( &m_processingParams );
                return gotDisplayPicture;
        }

        if( inputStream && inputStream->DataLength > 0 )
        {
            //QuickSync processes data by single NALU?
            if( inputStream->DataOffset == prevInputStreamDataOffset && inputStream->DataOffset > 0 )
            {
                //Quick Sync does not eat access_unit_delimiter at the end of a coded frame
                m_totalBytesDropped += inputStream->DataLength;
                cl_log.log( QString::fromAscii("Warning! Intel media decoder left %1 bytes of data (status %2). Dropping these bytes... Total bytes dropped %3").
                    arg(QString::number(inputStream->DataLength)).
                    arg(QString::number(opStatus)).
                    arg(QString::number(m_totalBytesDropped)),
                    cl_logINFO );
                //decoder didn't consume any data
                return gotDisplayPicture;
            }
            prevInputStreamDataOffset = inputStream->DataOffset;
            continue;
        }

        return gotDisplayPicture;
    }

    return gotDisplayPicture;
}

#ifndef XVBA_TEST
//!Not implemented yet
void QuickSyncVideoDecoder::setLightCpuMode( DecodeMode /*val*/ )
{
    //TODO/IMPL
}

//!Implementation of QnAbstractVideoDecoder::getWidth
int QuickSyncVideoDecoder::getWidth() const
{
    return m_srcStreamParam.mfx.FrameInfo.Width;
}

//!Implementation of QnAbstractVideoDecoder::getHeight
int QuickSyncVideoDecoder::getHeight() const
{
    return m_srcStreamParam.mfx.FrameInfo.Height;
}

//!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
double QuickSyncVideoDecoder::getSampleAspectRatio() const
{
    return m_srcStreamParam.mfx.FrameInfo.AspectRatioH > 0 
        ? ((double)m_srcStreamParam.mfx.FrameInfo.AspectRatioW / m_srcStreamParam.mfx.FrameInfo.AspectRatioH)
        : 1.0;
}

const AVFrame* QuickSyncVideoDecoder::lastFrame() const
{
    return m_lastAVFrame;
}

//!Reset decoder. Used for seek
void QuickSyncVideoDecoder::resetDecoder( QnCompressedVideoDataPtr data )
{
    if( m_decodingInitialized )
        m_decoder->Reset( &m_srcStreamParam );
}
#endif

void QuickSyncVideoDecoder::setOutPictureSize( const QSize& outSize )
{
    m_outPictureSize = QSize( ALIGN16(outSize.width()), ALIGN32(outSize.height()) );

    //creating processing
    m_processor.reset();
    if( m_decodingInitialized )
        initProcessor();
}

QnAbstractPictureData::PicStorageType QuickSyncVideoDecoder::targetMemoryType() const
{
    return QnAbstractPictureData::pstSysMemPic;
}

bool QuickSyncVideoDecoder::init( mfxU32 codecID, mfxBitstream* seqHeader )
{
    if( !m_mfxSessionEstablished && !initMfxSession() )
        return false;

    if( !m_decodingInitialized && !initDecoder( codecID, seqHeader ) )
        return false;

    if( processingNeeded() && !m_processor.get() && !initProcessor() )
        return false;

    return true;
}

bool QuickSyncVideoDecoder::initMfxSession()
{
    //opening media session
    mfxVersion version;
    memset( &version, 0, sizeof(version) );
    version.Major = 1;
    mfxStatus status = m_mfxSession.Init( MFX_IMPL_AUTO_ANY, &version );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to create Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }

    m_mfxSessionEstablished = true;

    return true;
}

bool QuickSyncVideoDecoder::initDecoder( mfxU32 codecID, mfxBitstream* seqHeader )
{
    Q_ASSERT( m_mfxSessionEstablished );

    if( !m_decoder.get() )
    {
        //initializing decoder
        m_decoder.reset( new MFXVideoDECODE( m_mfxSession ) );
    }

    mfxStatus status = MFX_ERR_NONE;
    m_srcStreamParam.mfx.CodecId = codecID;
    m_decoder->DecodeHeader( seqHeader, &m_srcStreamParam );
    switch( status )
    {
        case MFX_ERR_NONE:
            break;

        case MFX_ERR_MORE_DATA:
            cl_log.log( QString::fromAscii("Need more data to parse sequence header"), cl_logWARNING );
            return false;

        default:
            if( status > MFX_ERR_NONE )
            {
                //warning
                if( status == MFX_WRN_PARTIAL_ACCELERATION )
                {
                    cl_log.log( QString::fromAscii("Warning. Intel Media SDK can't use hardware acceleration for decoding"), cl_logWARNING );
                    m_hardwareAccelerationEnabled = false;
                }
                break;
            }
            cl_log.log( QString::fromAscii("Failed to decode stream header during Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            m_decoder.reset();
            return false;
    }

    cl_log.log( QString::fromAscii("Successfully decoded stream header with Intel Media decoder! Picture resolution %1x%2, stream profile/level: %3/%4").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)),
        cl_logINFO );

    //checking, whether hardware acceleration enabled
    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession.QueryIMPL( &actualImplUsed );
    m_hardwareAccelerationEnabled = actualImplUsed != MFX_IMPL_SOFTWARE;

#ifdef USE_D3D
    status = m_mfxSession.SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_d3dFrameAllocator.d3d9DeviceManager() );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to set IDirect3DDeviceManager9 pointer to MFX session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
#endif

    status = m_mfxSession.SetBufferAllocator( &m_bufAllocator );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to set buffer allocator to Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
    status = m_mfxSession.SetFrameAllocator( &m_frameAllocator );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to set frame allocator to Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }

    //m_srcStreamParam.AsyncDepth = std::max<>( m_srcStreamParam.AsyncDepth, (mfxU16)1 );
    m_srcStreamParam.AsyncDepth = 0;    //as in mfx example
    m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_srcStreamParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
#ifdef USE_D3D
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#else
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#endif

    status = m_decoder->Query( &m_srcStreamParam, &m_srcStreamParam );

    mfxFrameAllocRequest decodingAllocRequest;
    memset( &decodingAllocRequest, 0, sizeof(decodingAllocRequest) );
    //decodingAllocRequest.Info = m_srcStreamParam.mfx.FrameInfo;
    status = m_decoder->QueryIOSurf( &m_srcStreamParam, &decodingAllocRequest );
    if( status < MFX_ERR_NONE ) //ignoring warnings
    {
        cl_log.log( QString::fromAscii("Failed to query surface information for decoding from Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_decoder.reset();
        return false;
    }

#ifdef USE_D3D
    decodingAllocRequest.Type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
#else
    decodingAllocRequest.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
#endif
    decodingAllocRequest.Type |= MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_EXTERNAL_FRAME;

    allocateSurfacePool( &m_decoderSurfacePool, &decodingAllocRequest, &m_decodingAllocResponse );

    status = m_decoder->Init( &m_srcStreamParam );
    if( status < MFX_ERR_NONE )  //ignoring warnings
    {
        cl_log.log( QString::fromAscii("Failed to initialize Intel media SDK decoder. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_decoder.reset();
        return false;
    }

    cl_log.log( QString::fromAscii("Intel Media decoder successfully initialized! Picture resolution %1x%2, "
            "stream profile/level: %3/%4. Allocated %5 surfaces in %6 memory...").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)).
        arg(QString::number(m_decoderSurfacePool.size())).
#ifdef USE_D3D
        arg(QString::fromAscii("video")),
#else
        arg(QString::fromAscii("system")),
#endif
        cl_logINFO );

    //checking, whether hardware acceleration enabled
    m_mfxSession.QueryIMPL( &actualImplUsed );
    m_hardwareAccelerationEnabled = actualImplUsed != MFX_IMPL_SOFTWARE;

    m_decodingInitialized = true;
    return true;
}

bool QuickSyncVideoDecoder::initProcessor()
{
    Q_ASSERT( m_decodingInitialized );

    m_processor.reset( new MFXVideoVPP( m_mfxSession ) );

    //setting up re-scaling and de-interlace (if apropriate)
    memset( &m_processingParams, 0, sizeof(m_processingParams) );
    m_processingParams.AsyncDepth = 0;
#ifdef USE_D3D
    m_processingParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#else
    m_processingParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#endif
    m_processingParams.vpp.In = m_srcStreamParam.mfx.FrameInfo;
    m_processingParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    //m_processingParams.vpp.In.CropX = 0;
    //m_processingParams.vpp.In.CropY = 0;
    //m_processingParams.vpp.In.CropW = m_processingParams.vpp.In.Width;
    //m_processingParams.vpp.In.CropH = m_processingParams.vpp.In.Height;
    m_processingParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;    //MFX_PICSTRUCT_UNKNOWN;
    m_processingParams.vpp.In.FrameRateExtD = 30;  //must be not null
    m_processingParams.vpp.In.FrameRateExtN = 1;
    m_processingParams.vpp.Out = m_srcStreamParam.mfx.FrameInfo;
    m_processingParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    m_processingParams.vpp.Out.Width = m_outPictureSize.width();
    m_processingParams.vpp.Out.Height = m_outPictureSize.height();
    m_processingParams.vpp.Out.CropX = 0;
    m_processingParams.vpp.Out.CropY = 0;
    m_processingParams.vpp.Out.CropW = m_processingParams.vpp.Out.Width;
    m_processingParams.vpp.Out.CropH = m_processingParams.vpp.Out.Height;
    m_processingParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_processingParams.vpp.Out.FrameRateExtD = m_processingParams.vpp.In.FrameRateExtD;
    m_processingParams.vpp.Out.FrameRateExtN = m_processingParams.vpp.In.FrameRateExtN;

    mfxFrameAllocRequest request[2];
    memset( request, 0, sizeof(request) );

    mfxStatus status = m_processor->Query( &m_processingParams, &m_processingParams );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Warning! Failed to query VPP. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        return false;
    }

    status = m_processor->QueryIOSurf( &m_processingParams, request );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to query surface information for VPP from Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    //U00, V00, U01, V01, U10, V10, U11, V11;
    //U00, U01, U10, U11, V00, V01, V10, V11;
    //U00, U10, V00, V10, U01, U11, V01, V11;
    //U00, V00, U01, V01, U10, V10, U11, V11;

    //as input surfaces will use decoder output

    allocateSurfacePool( &m_processingSurfacePool, &request[1], &m_vppAllocResponse );
    for( int i = 0; i < m_processingSurfacePool.size(); ++i )
        m_processingSurfacePool[i].Info = m_processingParams.vpp.Out;

    status = m_processor->Init( &m_processingParams );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to initialize Intel media SDK processor. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    cl_log.log( QString::fromAscii("Intel Media processor (scale to %1x%2) successfully initialized!. Allocated %3 surfaces in %4 memory").
        arg(m_outPictureSize.width()).arg(m_outPictureSize.height()).
        arg(m_processingSurfacePool.size()).
#ifdef USE_D3D
        arg(QString::fromAscii("video")),
#else
        arg(QString::fromAscii("system")),
#endif
        cl_logINFO );

    return true;
}

void QuickSyncVideoDecoder::allocateSurfacePool(
    std::vector<mfxFrameSurface1>* const surfacePool,
    mfxFrameAllocRequest* const allocRequest,
    mfxFrameAllocResponse* const allocResponse )
{
    memset( allocResponse, 0, sizeof(*allocResponse) );
    m_frameAllocator.alloc( allocRequest, allocResponse );
    initializeSurfacePoolFromAllocResponse( surfacePool, *allocResponse );
}

void QuickSyncVideoDecoder::initializeSurfacePoolFromAllocResponse(
    std::vector<mfxFrameSurface1>* const surfacePool,
    const mfxFrameAllocResponse& allocResponse )
{
    surfacePool->reserve( surfacePool->size() + allocResponse.NumFrameActual );
    for( int i = 0; i < allocResponse.NumFrameActual; ++i )
    {
        surfacePool->push_back( mfxFrameSurface1() );
        memset( &surfacePool->back(), 0, sizeof(mfxFrameSurface1) );
        memcpy( &surfacePool->back().Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
        surfacePool->back().Data.MemId = allocResponse.mids[i];
    }
}

mfxFrameSurface1* QuickSyncVideoDecoder::findUnlockedSurface( std::vector<mfxFrameSurface1>* const surfacePool )
{
    for( int i = 0; i < surfacePool->size(); ++i )
    {
        if( surfacePool->at(i).Data.Locked == 0 )
            return &surfacePool->at(i);
    }
    Q_ASSERT( false );
    return NULL;
    //TODO/IMPL allocating memory for one more frame
    //m_decoderSurfacePool.push_back( mfxFrameSurface1() );
    //memset( &m_decoderSurfacePool.back(), 0, sizeof(mfxFrameSurface1) );
    //memcpy( &m_decoderSurfacePool.back().Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
    //return &m_decoderSurfacePool.back();
}

bool QuickSyncVideoDecoder::processingNeeded() const
{
    return !m_outPictureSize.isNull();
}

void QuickSyncVideoDecoder::saveToAVFrame( CLVideoDecoderOutput* outFrame, mfxFrameSurface1* decodedFrame )
{
    if( !outFrame->isExternalData() )
    {
        //copying frame data if needed
        outFrame->reallocate( decodedFrame->Data.Pitch, decodedFrame->Info.Height, PIX_FMT_NV12 );
        memcpy( outFrame->data[0], decodedFrame->Data.Y, decodedFrame->Data.Pitch * decodedFrame->Info.Height );
        memcpy( outFrame->data[1], decodedFrame->Data.U, decodedFrame->Data.Pitch * decodedFrame->Info.Height / 2 );
    }
    else
    {
        outFrame->data[0] = decodedFrame->Data.Y;
        outFrame->data[1] = decodedFrame->Data.U;
        outFrame->data[2] = decodedFrame->Data.V;
    }
    outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
    outFrame->linesize[1] = decodedFrame->Data.Pitch;       //uv_stride
    outFrame->linesize[2] = 0;

    outFrame->width = decodedFrame->Info.Width;
    outFrame->height = decodedFrame->Info.Height;
    outFrame->format = PIX_FMT_NV12;
    //outFrame->key_frame = decodedFrame->Data.
    //outFrame->pts = av_rescale_q( decodedFrame->Data.TimeStamp, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION, SRC_DATA_TIMESTAMP_RESOLUTION );
    outFrame->pts = decodedFrame->Data.TimeStamp;
    outFrame->pkt_dts = decodedFrame->Data.TimeStamp;

    outFrame->display_picture_number = decodedFrame->Data.FrameOrder;
    outFrame->interlaced_frame = 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_REPEATED;
}
/*
bool QuickSyncVideoDecoder::readSequenceHeader( const QnCompressedVideoDataPtr& data )
{
    SPSUnit m_sps;
    PPSUnit m_pps;

    if( data->data.size() <= 4 )
        return false;

    //memset( &m_pictureDescriptor, 0, sizeof(m_pictureDescriptor) );

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
                //qDebug()<<"Parsing sps\n";
                m_sps.decodeBuffer( curNalu, nextNalu );
                m_sps.deserialize();
                spsFound = true;

                //m_srcStreamParam.mfx.CodecId = MFX_CODEC_AVC;
                //m_srcStreamParam.mfx.CodecProfile = m_sps.profile_idc;
                //m_srcStreamParam.mfx.CodecLevel = m_sps.level_idc;

                //m_srcStreamParam.mfx.TimeStampCalc = MFX_TIMESTAMPCALC_UNKNOWN;
                //m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                //m_srcStreamParam.mfx.FrameInfo.Width = m_sps.pic_width_in_mbs * 16;
                //m_srcStreamParam.mfx.FrameInfo.Height = m_sps.pic_height_in_map_units * 16;
                //m_srcStreamParam.mfx.FrameInfo.CropX = 0;
                //m_srcStreamParam.mfx.FrameInfo.CropY = 0;
                //m_srcStreamParam.mfx.FrameInfo.CropW = m_srcStreamParam.mfx.FrameInfo.Width;
                //m_srcStreamParam.mfx.FrameInfo.CropH = m_srcStreamParam.mfx.FrameInfo.Height;
                //m_srcStreamParam.mfx.FrameInfo.FrameRateExtN = 0;
                //m_srcStreamParam.mfx.FrameInfo.FrameRateExtD = 0;
                //m_srcStreamParam.mfx.FrameInfo.ChromaFormat = m_sps.chroma_format_idc;
                //m_srcStreamParam.mfx.FrameInfo.AspectRatioW = m_srcStreamParam.mfx.FrameInfo.Width;
                //m_srcStreamParam.mfx.FrameInfo.AspectRatioH = m_srcStreamParam.mfx.FrameInfo.Height;

                //m_pictureDescriptor.profile = m_sps.profile_idc;
                //m_pictureDescriptor.level = m_sps.level_idc;
                //m_pictureDescriptor.width_in_mb = m_sps.pic_width_in_mbs;
                //m_pictureDescriptor.height_in_mb = m_sps.pic_height_in_map_units;
                //m_pictureDescriptor.sps_info.avc.residual_colour_transform_flag = m_sps.residual_colour_transform_flag;
                //m_pictureDescriptor.sps_info.avc.delta_pic_always_zero_flag = m_sps.delta_pic_order_always_zero_flag;
                //m_pictureDescriptor.sps_info.avc.gaps_in_frame_num_value_allowed_flag = m_sps.gaps_in_frame_num_value_allowed_flag;
                //m_pictureDescriptor.sps_info.avc.frame_mbs_only_flag = m_sps.frame_mbs_only_flag;
                //m_pictureDescriptor.sps_info.avc.mb_adaptive_frame_field_flag = m_sps.mb_adaptive_frame_field_flag;
                //m_pictureDescriptor.sps_info.avc.direct_8x8_inference_flag = m_sps.direct_8x8_inference_flag;

                //m_pictureDescriptor.chroma_format = m_sps.chroma_format_idc;
                //m_pictureDescriptor.avc_bit_depth_luma_minus8 = m_sps.bit_depth_luma - 8;
                //m_pictureDescriptor.avc_bit_depth_chroma_minus8 = m_sps.bit_depth_chroma - 8;
                //m_pictureDescriptor.avc_log2_max_frame_num_minus4 = m_sps.log2_max_frame_num - 4;

                //m_pictureDescriptor.avc_pic_order_cnt_type = m_sps.pic_order_cnt_type;
                //m_pictureDescriptor.avc_log2_max_pic_order_cnt_lsb_minus4 = m_sps.log2_max_pic_order_cnt_lsb;
                //m_pictureDescriptor.avc_num_ref_frames = m_sps.num_ref_frames;
                break;
            }

            case nuPPS:
            {
                //qDebug()<<"Parsing pps\n";
                m_pps.decodeBuffer( curNalu, nextNalu );
                m_pps.deserialize();
                ppsFound = true;

                //m_pictureDescriptor.avc_num_slice_groups_minus1 = m_pps.num_slice_groups_minus1;
                //m_pictureDescriptor.avc_slice_group_map_type = m_pps.slice_group_map_type;
                //m_pictureDescriptor.avc_num_ref_idx_l0_active_minus1 = m_pps.num_ref_idx_l0_active_minus1;
                //m_pictureDescriptor.avc_num_ref_idx_l1_active_minus1 = m_pps.num_ref_idx_l1_active_minus1;

                //m_pictureDescriptor.avc_pic_init_qp_minus26 = m_pps.pic_init_qp_minus26;
                //m_pictureDescriptor.avc_pic_init_qs_minus26 = m_pps.pic_init_qs_minus26;
                //m_pictureDescriptor.avc_chroma_qp_index_offset = m_pps.chroma_qp_index_offset;
                //m_pictureDescriptor.avc_second_chroma_qp_index_offset = m_pps.second_chroma_qp_index_offset;

                //m_pictureDescriptor.avc_slice_group_change_rate_minus1 = m_pps.slice_group_change_rate - 1;

                //m_pictureDescriptor.pps_info.avc.entropy_coding_mode_flag = m_pps.entropy_coding_mode_flag;
                //m_pictureDescriptor.pps_info.avc.pic_order_present_flag = m_pps.pic_order_present_flag;
                //m_pictureDescriptor.pps_info.avc.weighted_pred_flag = m_pps.weighted_pred_flag;
                //m_pictureDescriptor.pps_info.avc.weighted_bipred_idc = m_pps.weighted_bipred_idc;
                //m_pictureDescriptor.pps_info.avc.deblocking_filter_control_present_flag = m_pps.deblocking_filter_control_present_flag;
                //m_pictureDescriptor.pps_info.avc.constrained_intra_pred_flag = m_pps.constrained_intra_pred_flag;
                //m_pictureDescriptor.pps_info.avc.redundant_pic_cnt_present_flag = m_pps.redundant_pic_cnt_present_flag;
                //m_pictureDescriptor.pps_info.avc.transform_8x8_mode_flag = m_pps.transform_8x8_mode_flag;
                break;
            }

            default:
                break;
        }
    }

    return spsFound && ppsFound;
}
*/
QString QuickSyncVideoDecoder::mfxStatusCodeToString( mfxStatus status ) const
{
    switch( status )
    {
        case MFX_ERR_NONE:
            return QString::fromAscii("no error");

        /* reserved for unex*pected errors */
        case MFX_ERR_UNKNOWN:
            return QString::fromAscii("unknown error");

        /* error codes <0 */
        case MFX_ERR_NULL_PTR:
            return QString::fromAscii("null pointer");
        case MFX_ERR_UNSUPPORTED:
            return QString::fromAscii("undeveloped feature");
        case MFX_ERR_MEMORY_ALLOC:
            return QString::fromAscii("failed to allocate memory");
        case MFX_ERR_NOT_ENOUGH_BUFFER:
            return QString::fromAscii("insufficient buffer at input");
        case MFX_ERR_INVALID_HANDLE:
            return QString::fromAscii("invalid handle");
        case MFX_ERR_LOCK_MEMORY:
            return QString::fromAscii("failed to lock the memory block");
        case MFX_ERR_NOT_INITIALIZED:
            return QString::fromAscii("member function called before initialization");
        case MFX_ERR_NOT_FOUND:
            return QString::fromAscii("the specified object is not found");
        case MFX_ERR_MORE_DATA:
            return QString::fromAscii("expect more data at input");
        case MFX_ERR_MORE_SURFACE:
            return QString::fromAscii("expect more surface at output");
        case MFX_ERR_ABORTED:
            return QString::fromAscii("operation aborted");
        case MFX_ERR_DEVICE_LOST:
            return QString::fromAscii("lose the HW acceleration device");
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            return QString::fromAscii("incompatible video parameters");
        case MFX_ERR_INVALID_VIDEO_PARAM:
            return QString::fromAscii("invalid video parameters");
        case MFX_ERR_UNDEFINED_BEHAVIOR:
            return QString::fromAscii("undefined behavior");
        case MFX_ERR_DEVICE_FAILED:
            return QString::fromAscii("device operation failure");
        case MFX_ERR_MORE_BITSTREAM:
            return QString::fromAscii("expect more bitstream buffers at output");

        /* warnings >0 */
        case MFX_WRN_IN_EXECUTION:
            return QString::fromAscii("the previous asynchrous operation is in execution");
        case MFX_WRN_DEVICE_BUSY:
            return QString::fromAscii("the HW acceleration device is busy");
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            return QString::fromAscii("the video parameters are changed during decoding");
        case MFX_WRN_PARTIAL_ACCELERATION:
            return QString::fromAscii("SW is used");
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
            return QString::fromAscii("incompatible video parameters");
        case MFX_WRN_VALUE_NOT_CHANGED:
            return QString::fromAscii("the value is saturated based on its valid range");
        case MFX_WRN_OUT_OF_RANGE:
            return QString::fromAscii("the value is out of valid range");

        /* threading statuses */
        case MFX_TASK_WORKING:
            return QString::fromAscii("there is some more work to do");
        case MFX_TASK_BUSY:
            return QString::fromAscii("task is waiting for resources");
        default:
            return QString::fromAscii("unknown error");
    }
}
