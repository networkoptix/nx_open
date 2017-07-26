#include <common/config.h>

#include "quicksyncvideodecoder.h"

#include <intrin.h>

#include <Winsock2.h>
#include <dxerr.h>

#include <algorithm>

#ifdef USE_OPENCL
#define DX9_SHARING
#define DX9_MEDIA_SHARING
#include <CL/cl.h>
#include <CL/cl_d3d9.h>
#include <CL/cl_gl.h>
#endif
#ifndef XVBA_TEST
#include <libavcodec/avcodec.h>
#endif
#include <QDebug>
#include <QElapsedTimer>

#ifndef XVBA_TEST
#include <plugins/videodecoders/pluginusagewatcher.h>
#include <plugins/videodecoders/videodecoderplugintypes.h>
#include <plugins/videodecoders/abstract_decoder_event_receiver.h>
#else
quint64 getUsecTimer()
{
    return 0;
}
#endif

#include "mfxframeinfo_resource_reader.h"

//#define USE_SYSMEM_SURFACE
#ifndef USE_SYSMEM_SURFACE
#define USE_D3D_SURFACE
#endif

#ifdef USE_D3D_SURFACE
static const size_t SCALED_SURFACE_COUNT = 3;
#endif
//!if defined, than stream, coded in ffmpeg format (nalu_size,nalu) converted to h.264 annexb right in input buffer, otherwise - copied to temporary buffer
#define ALLOW_INPLACE_CONVERSION_TO_ANNEXB


static const quint8 H264_START_CODE[] = { 0, 0, 0, 1 };

//#define OWN_NUM_FRAMES_CALCULATION

//TODO/IMPL take sequence header from extradata. Test on file
//TODO/IMPL support last frame
//TODO/IMPL make decoder asynchronous. Fix visual artifacts
//TODO/IMPL make common scaled surface pool for efficient video memory usage: we'll have to use common d3d device - but this causes increase of CPU usage
//TODO/IMPL reduce decoder initialization time:
    //create pool of MFX sessions
//TODO/IMPL
    //delayed scaled frame surface creation (low priority)
    //delayed decoder frame surface creation (high priority), since decoder does not use all surfaces - impossible

QuickSyncVideoDecoder::QuicksyncDXVAPictureData::QuicksyncDXVAPictureData(
    AbstractMFXFrameAllocator* const allocator,
    const QSharedPointer<SurfaceContext>& surface,
    const QRect& _cropRect,
    QuickSyncVideoDecoder* decoder )
:
    D3DPictureData( &surface->syncCtx ),
    m_mfxSurface( surface ),
    m_d3dSurface( NULL ),
    m_cropRect( _cropRect ),
    m_decoder( decoder )
{
    ++surface->syncCtx.externalRefCounter;
    allocator->gethdl( m_mfxSurface->mfxSurface.Data.MemId, reinterpret_cast<mfxHDL*>(&m_d3dSurface) );
    ULONG refCount = m_d3dSurface->AddRef();
    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder::QuicksyncDXVAPictureData::QuicksyncDXVAPictureData(). surf(%1) ref %2").
        arg((size_t)m_d3dSurface, 0, 16).arg(refCount), cl_logDEBUG2 );
}

QuickSyncVideoDecoder::QuicksyncDXVAPictureData::~QuicksyncDXVAPictureData()
{
    ULONG refCount = m_d3dSurface->Release();
    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder::QuicksyncDXVAPictureData::~QuicksyncDXVAPictureData(). surf(%1) ref %2").
        arg((size_t)m_d3dSurface, 0, 16).arg(refCount), cl_logDEBUG2 );
    --m_mfxSurface->syncCtx.externalRefCounter;
}

IDirect3DSurface9* QuickSyncVideoDecoder::QuicksyncDXVAPictureData::getSurface() const
{
    return m_d3dSurface;
}

QRect QuickSyncVideoDecoder::QuicksyncDXVAPictureData::cropRect() const
{
    return m_cropRect;
}

const QSharedPointer<QuickSyncVideoDecoder::SurfaceContext>& QuickSyncVideoDecoder::QuicksyncDXVAPictureData::getSurfaceContext() const
{
    return m_mfxSurface;
}


//-------------------------------------------------------------------------------------------------
// QuickSyncVideoDecoder

//!Maximum tries to decode frame
static const int MAX_ENCODE_ASYNC_CALL_TRIES = 7;
static const int MS_TO_WAIT_FOR_DEVICE = 5;
//!Timeout to next check of async operation(s) for completion
static const unsigned int ASYNC_OPERATION_WAIT_MS = 5;

#ifdef USE_ASYNC_IMPL
QuickSyncVideoDecoder::AsyncOperationContext::AsyncOperationContext()
:
    decodedFrameMfxSurf( NULL ),
    syncPoint( NULL ),
    state( issDecodeInProgress ),
    prevOperationResult( MFX_ERR_NONE )
{
}
#endif

QuickSyncVideoDecoder::QuickSyncVideoDecoder(
    MFXVideoSession* const parentSession,
    IDirect3DDeviceManager9* d3d9manager,
    const QnCompressedVideoDataPtr data,
    PluginUsageWatcher* const pluginUsageWatcher,
    AbstractDecoderEventReceiver* const eventReceiver,
    unsigned int adapterNumber )
:
    m_parentSession( parentSession ),
    m_pluginUsageWatcher( pluginUsageWatcher ),
    m_eventReceiver( eventReceiver ),
    m_state( notInitialized ),
    m_syncPoint( NULL ),
    m_mfxSessionEstablished( false ),
    m_decodingInitialized( false ),
    m_totalBytesDropped( 0 ),
    m_prevTimestamp( 0 ),
    m_decoderSurfaceSizeInBytes( 0 ),
    m_scaledSurfaceSizeInBytes( 0 ),
    m_d3dFrameAllocator( d3d9manager, adapterNumber ),
    m_frameAllocator( &m_sysMemFrameAllocator, &m_d3dFrameAllocator ),
    m_originalFrameCropTop( 0 ),
    m_originalFrameCropBottom( 0 ),
    m_frameCropTopActual( 0 ),
    m_frameCropBottomActual( 0 ),
    m_reinitializeNeeded( false ),
    m_sourceStreamFps( 0 ),
    m_totalInputFrames( 0 ),
    m_totalOutputFrames( 0 ),
    m_prevInputFrameMs( (DWORD)-1 ),
    m_prevOutPictureClock( 0 ),
    m_recursionDepth( 0 ),
    m_speed( 1.0 ),
    m_initializationMode( false ),
    m_streamIsAnnexB( true ),
    m_nalLengthSize( 0 ),
    m_lastFrameInSysMem( nullptr )
#ifdef USE_OPENCL
    ,m_clContext( NULL ),
    m_prevCLStatus( CL_SUCCESS ),
    m_clGetDeviceIDsFromDX9INTEL( NULL ),
    m_clCreateFromDX9MediaSurfaceINTEL( NULL ),
    m_glContextDevice( NULL ),
    m_glContext( NULL )
#endif
{
    memset( &m_srcStreamParam, 0, sizeof(m_srcStreamParam) );
    memset( &m_decodingAllocResponse, 0, sizeof(m_decodingAllocResponse) );
    memset( &m_vppAllocResponse, 0, sizeof(m_vppAllocResponse) );

#ifndef USE_SYSMEM_SURFACE
    if( !m_d3dFrameAllocator.initialize() )
    {
        NX_LOG( QString::fromLatin1("Failed to initialize DXVA frame surface allocator. %1").arg(m_d3dFrameAllocator.getLastErrorText()), cl_logERROR );
        return;
    }
#endif

#ifdef USE_OPENCL
    m_glContextDevice = m_d3dFrameAllocator.dc();
    m_glContext = wglCreateContext( m_glContextDevice );
    if( m_glContext == NULL )
    {
        DWORD errorCode = GetLastError();
        LPVOID lpMsgBuf = NULL;
        if( FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf,
                0,
                NULL ) == 0 )
        {
            errorCode = GetLastError();
        }
        LocalFree(lpMsgBuf);
        return;
    }
#endif

#if defined(WRITE_INPUT_STREAM_TO_FILE_1) || defined(WRITE_INPUT_STREAM_TO_FILE_2)
    m_inputStreamFile.open( "C:/temp/in.264", std::ios::binary );
#endif
#ifdef WRITE_INPUT_STREAM_TO_FILE_1
    m_inputStreamFile.write( data->data.data(), data->data.size() );
#endif

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    inputStream.TimeStamp = data->timestamp;
    inputStream.Data = reinterpret_cast<mfxU8*>(const_cast<char*>(data->data.data()));
    inputStream.DataLength = data->data.size();
    inputStream.MaxLength = data->data.size();

    //init( MFX_CODEC_AVC, &inputStream );
    decode( &inputStream, NULL );
}


QuickSyncVideoDecoder::QuickSyncVideoDecoder(
    MFXVideoSession* const parentSession,
    IDirect3DDeviceManager9* d3d9manager,
    PluginUsageWatcher* const pluginUsageWatcher,
    AbstractDecoderEventReceiver* const eventReceiver,
    unsigned int adapterNumber )
:
    m_parentSession( parentSession ),
    m_pluginUsageWatcher( pluginUsageWatcher ),
    m_eventReceiver( eventReceiver ),
    m_state( notInitialized ),
    m_syncPoint( NULL ),
    m_mfxSessionEstablished( false ),
    m_decodingInitialized( false ),
    m_totalBytesDropped( 0 ),
    m_prevTimestamp( 0 ),
    m_decoderSurfaceSizeInBytes( 0 ),
    m_scaledSurfaceSizeInBytes( 0 ),
    m_d3dFrameAllocator( d3d9manager, adapterNumber ),
    m_frameAllocator( &m_sysMemFrameAllocator, &m_d3dFrameAllocator ),
    m_originalFrameCropTop( 0 ),
    m_originalFrameCropBottom( 0 ),
    m_frameCropTopActual( 0 ),
    m_frameCropBottomActual( 0 ),
    m_reinitializeNeeded( false ),
    m_sourceStreamFps( 0 ),
    m_totalInputFrames( 0 ),
    m_totalOutputFrames( 0 ),
    m_prevInputFrameMs( (DWORD)-1 ),
    m_prevOutPictureClock( 0 ),
    m_recursionDepth( 0 ),
    m_speed( 1.0 ),
    m_initializationMode( false ),
    m_streamIsAnnexB( true ),
    m_nalLengthSize( 0 ),
    m_lastFrameInSysMem( nullptr )
#ifdef USE_OPENCL
    ,m_clContext( NULL ),
    m_prevCLStatus( CL_SUCCESS ),
    m_clGetDeviceIDsFromDX9INTEL( NULL ),
    m_clCreateFromDX9MediaSurfaceINTEL( NULL ),
    m_glContextDevice( NULL ),
    m_glContext( NULL )
#endif
{
    memset( &m_srcStreamParam, 0, sizeof(m_srcStreamParam) );
    memset( &m_decodingAllocResponse, 0, sizeof(m_decodingAllocResponse) );
    memset( &m_vppAllocResponse, 0, sizeof(m_vppAllocResponse) );

#ifndef USE_SYSMEM_SURFACE
    DWORD t1 = GetTickCount();
    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder initialization start %1").arg(t1), cl_logDEBUG1 );
    if( !m_d3dFrameAllocator.initialize() )
    {
        NX_LOG( QString::fromLatin1("Failed to initialize DXVA frame surface allocator. %1").arg(m_d3dFrameAllocator.getLastErrorText()), cl_logERROR );
        return;
    }
    DWORD t2 = GetTickCount();
    NX_LOG( QString::fromLatin1("DXVA frame surface allocator initialized in %1 ms").arg(t2-t1), cl_logDEBUG1 );
#endif

#ifdef USE_OPENCL
    m_glContextDevice = m_d3dFrameAllocator.dc();
    m_glContext = wglCreateContext( m_glContextDevice );
    if( m_glContext == NULL )
    {
        DWORD errorCode = GetLastError();
        LPVOID lpMsgBuf = NULL;
        if( FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf,
                0,
                NULL ) == 0 )
        {
            errorCode = GetLastError();
        }
        LocalFree(lpMsgBuf);
        return;
    }
#endif

#if defined(WRITE_INPUT_STREAM_TO_FILE_1) || defined(WRITE_INPUT_STREAM_TO_FILE_2)
    m_inputStreamFile.open( "C:/temp/in.264", std::ios::binary );
#endif
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
    closeMFXSession();

#ifdef USE_OPENCL
    clReleaseContext( m_clContext );
#endif

#ifndef XVBA_TEST
    m_pluginUsageWatcher->decoderIsAboutToBeDestroyed( this );
#endif
}

PixelFormat QuickSyncVideoDecoder::GetPixelFormat() const
{
#ifndef CONVERT_NV12_TO_YV12
    return AV_PIX_FMT_NV12;
#else
    return AV_PIX_FMT_YUV420P;
#endif
}

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif


#ifndef XVBA_TEST
static const AVRational SRC_DATA_TIMESTAMP_RESOLUTION = {1, 1000000};
static const AVRational INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION = {1, 90000};
#endif
static const int MICROS_IN_SECOND = 1000*1000;

bool QuickSyncVideoDecoder::decode( const QnConstCompressedVideoDataPtr data, QSharedPointer<CLVideoDecoderOutput>* const outFrame )
{
    const DWORD currentClock = GetTickCount();

    bool convertToAnnexBInPlace = false;

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    if( data && data->data.size() > 0 )
    {
        NX_ASSERT( data->compressionType == AV_CODEC_ID_H264 );

        const DWORD millisSincePrevInputFrame = (m_prevInputFrameMs != (DWORD)-1) ? (GetTickCount() - m_prevInputFrameMs) : 0;
        const bool delayedInputFrame = millisSincePrevInputFrame > (m_sourceStreamFps > 0 ? 1000 / m_sourceStreamFps : 50)*3;
        NX_LOG( QString("QuickSyncVideoDecoder::decode. data.size = %1, current fps = %2, timer %3, millis since prev input frame %4%5").
            arg(data->data.size()).arg(m_sourceStreamFps).arg(getUsecTimer()/1000).arg(millisSincePrevInputFrame).
            arg(QString::fromLatin1(delayedInputFrame ? ". Delayed input frame" : "")), cl_logDEBUG2 );

        ++m_totalInputFrames;

        static const size_t FRAME_COUNT_TO_CALC_FPS = 20;
        static const size_t MIN_FRAME_COUNT_TO_CALC_FPS = FRAME_COUNT_TO_CALC_FPS / 2;

        //calculating m_sourceStreamFps
        const double fpsCalcTimestampDiff = m_fpsCalcFrameTimestamps.empty()
            ? 0
            : m_fpsCalcFrameTimestamps.back() - m_fpsCalcFrameTimestamps.front(); //TODO overflow?
        if( (m_fpsCalcFrameTimestamps.size() >= MIN_FRAME_COUNT_TO_CALC_FPS) && (fpsCalcTimestampDiff > 0) )
            m_sourceStreamFps = m_fpsCalcFrameTimestamps.size() * MICROS_IN_SECOND / fpsCalcTimestampDiff;
        m_fpsCalcFrameTimestamps.push_back( data->timestamp );
        if( m_fpsCalcFrameTimestamps.size() > FRAME_COUNT_TO_CALC_FPS )
            m_fpsCalcFrameTimestamps.pop_front();

    #ifdef WRITE_INPUT_STREAM_TO_FILE_1
        m_inputStreamFile.write( data->data.data(), data->data.size() );
    #endif

        inputStream.TimeStamp = data->timestamp;

    #ifndef XVBA_TEST
        if( (m_state < decoding) && isH264SeqHeaderInExtraData(data) )
        {
            std::basic_string<mfxU8> seqHeader;
            readH264SeqHeaderFromExtraData( data, &seqHeader );
            m_streamIsAnnexB = false;
            inputStream.Data = const_cast<mfxU8*>(seqHeader.data());
            inputStream.DataLength = seqHeader.size();
            inputStream.MaxLength = seqHeader.size();
            const bool initializationModeBak = m_initializationMode;
            m_initializationMode = false;
            const bool result = decode( &inputStream, outFrame );
            m_initializationMode = initializationModeBak;
            return result;
        }
    #endif

        //qDebug()<<"Input timestamp: "<<inputStream.TimeStamp;
        //if( m_prevTimestamp > inputStream.TimeStamp )
        //    qDebug()<<"Warning! timestamp decreased by "<<(m_prevTimestamp - inputStream.TimeStamp);
        //m_prevTimestamp = inputStream.TimeStamp;

    #ifndef XVBA_TEST
        inputStream.Data = const_cast<mfxU8*>(reinterpret_cast<const mfxU8*>(data->data.data()));   //todo: #ak const_cast is really required?
    #else
        inputStream.Data = reinterpret_cast<mfxU8*>(const_cast<char*>(data->data.data()));
    #endif
        inputStream.DataLength = data->data.size();
        inputStream.MaxLength = data->data.size();
        //inputStream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

        if( !m_streamIsAnnexB )
        {
            m_tempFrameBuffer.resize( 0 );
#ifdef ALLOW_INPLACE_CONVERSION_TO_ANNEXB
            convertToAnnexBInPlace = m_nalLengthSize >= 3;
#else
            convertToAnnexBInPlace = false;
#endif

            //decoding ffmpeg proprietary format (removing size before each nalu and inserting H264_START_CODE)
            for( mfxU8*
                p = inputStream.Data;
                p < inputStream.Data + inputStream.DataLength;
                 )
            {
                int naluSize = 0;
                for( int i = 0; i < m_nalLengthSize; ++i )
                    naluSize = (naluSize << 8) | p[i];
                if( p + m_nalLengthSize + naluSize > inputStream.Data + inputStream.DataLength )
                {
                    //corrupted data?
                    break;
                }

#ifdef ALLOW_INPLACE_CONVERSION_TO_ANNEXB
                if( convertToAnnexBInPlace )
                {
                    //performing in-place stream modification
                    //adding start code prefix
                    mfxU8* placeForStartCode = p;
                    for( int i = 0; i < m_nalLengthSize-1; ++i )
                        *(placeForStartCode++) = 0;
                    *placeForStartCode = 1;
                }
                else
#endif
                {
                    m_tempFrameBuffer.append( H264_START_CODE, sizeof(H264_START_CODE) );
                    m_tempFrameBuffer.append( p + m_nalLengthSize, naluSize );
                }

                p += m_nalLengthSize + naluSize;
            }

            if( !convertToAnnexBInPlace )
            {
                inputStream.Data = const_cast<mfxU8*>(m_tempFrameBuffer.data());    // TODO: #ak give up const_cast after move to c++11
                inputStream.DataLength = m_tempFrameBuffer.size();
                inputStream.MaxLength = m_tempFrameBuffer.size();
            }
        }

        if( m_prevInputFrameMs == (DWORD)-1 )
            m_prevOutPictureClock = GetTickCount();

        //saving motion info
        m_srcMotionInfo[data->timestamp] = data->motion;
        if( m_srcMotionInfo.size() > m_decodingAllocResponse.NumFrameActual + m_vppAllocResponse.NumFrameActual + 1U )
        {
            //removing old motion info
            for( MotionInfoContainerType::size_type
                i = 0;
                i < (m_srcMotionInfo.size() - (m_decodingAllocResponse.NumFrameActual + m_vppAllocResponse.NumFrameActual + 1));
                ++i )
            {
                m_srcMotionInfo.erase( m_srcMotionInfo.begin() );
            }
        }
    }
    else
    {
        //NULL input data
        if( m_state < decoding )
            return false;
    }

    const bool decoderProducedFrame = decode( &inputStream, outFrame );

#ifdef ALLOW_INPLACE_CONVERSION_TO_ANNEXB
    if( !m_streamIsAnnexB && !convertToAnnexBInPlace )
    {
        //TODO/IMPL returning source data to initial state: replacing start_code with nalu length
    }
#endif

    if( decoderProducedFrame )
    {
        if( m_totalOutputFrames == 0 )
            NX_LOG( QString::fromLatin1( "QuickSyncVideoDecoder. First decoded frame delayed for %1 input frame(s). clock %2" ).arg(m_totalInputFrames-1).arg(GetTickCount()), cl_logDEBUG1 );
        ++m_totalOutputFrames;
        if( currentClock - m_prevOutPictureClock > 1000 )
            NX_LOG( QString::fromLatin1( "QuickSyncVideoDecoder. Warning! There was no out picture for %1 ms" ).arg(currentClock - m_prevOutPictureClock), cl_logDEBUG1 );
        m_prevOutPictureClock = currentClock;

        if( *outFrame )
        {
            MotionInfoContainerType::const_iterator motionIter = m_srcMotionInfo.find( (*outFrame)->pkt_dts );
            if( motionIter != m_srcMotionInfo.end() )
                (*outFrame)->metadata = motionIter->second;
        }
    }

    //NX_LOG( QString::fromLatin1( "QuickSyncVideoDecoder::decode. exit. timer %1, lr-timer %2" ).arg(currentClock).arg(GetTickCount()), cl_logDEBUG1 );
    m_prevInputFrameMs = GetTickCount();

    //return m_totalOutputFrames > 5 ? false : decoderProducedFrame;
    return decoderProducedFrame;
}

#ifndef USE_ASYNC_IMPL
bool QuickSyncVideoDecoder::decode( mfxBitstream* const inputStream, QSharedPointer<CLVideoDecoderOutput>* const outFrame )
{
    mfxVideoParam newStreamParams;
    memset( &newStreamParams, 0, sizeof(newStreamParams) );
    const bool isSeqHeaderPresent = readSequenceHeader( inputStream, &newStreamParams ) == MFX_ERR_NONE;

    if( m_state > notInitialized && m_reinitializeNeeded && isSeqHeaderPresent )
    {
        //have to wait for a sequence header and I-frame before reinitializing decoder
        closeMFXSession();
        m_reinitializeNeeded = false;

        if( m_eventReceiver )
        {
            AbstractDecoderEventReceiver::DecoderBehaviour futherLifeStyle = m_eventReceiver->streamParamsChanged(
                this, MFXFrameInfoResourceReader(newStreamParams.mfx.FrameInfo) );
            if( futherLifeStyle == AbstractDecoderEventReceiver::dbStop )
                return false;
        }
    }
    if( m_state < decoding && !init( MFX_CODEC_AVC, inputStream ) )
        return false;

    if( m_initializationMode )
        return false;

    bool gotDisplayPicture = false;

    mfxFrameSurface1* decodedFrame = NULL;
    QSharedPointer<SurfaceContext> decodedFrameCtx;
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
                    opStatus = doDecodingStep( inputStream, &decodedFrame );
                    break;

#ifndef USE_SYSMEM_SURFACE
                case processing:
                    opStatus = doProcessingStep( decodedFrameCtx, &decodedFrameCtx );
                    break;
#endif  //USE_SYSMEM_SURFACE
            }

            if( opStatus >= MFX_ERR_NONE && m_syncPoint )
            {
                opStatus = m_mfxSession.SyncOperation( m_syncPoint, INFINITE );
                NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. sync operation result %1").arg(mfxStatusCodeToString(opStatus)),
                    opStatus == MFX_ERR_NONE ? cl_logDEBUG2 : cl_logDEBUG1 );

                if( decodedFrame && !decodedFrameCtx.data() )
                    decodedFrameCtx = getSurfaceCtxFromSurface( decodedFrame );
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
#ifndef USE_SYSMEM_SURFACE
#ifdef SCALE_WITH_MFX
                    m_state = m_processor.get() ? processing : done;
#else
                    m_state = processingNeeded() ? processing : done;
#endif  //SCALE_WITH_MFX
#else   //USE_SYSMEM_SURFACE
                    m_state = done;
#endif
                    break;

                case processing:
                    m_state = done;
                    break;
            }
        }

        switch( opStatus )
        {
            case MFX_ERR_NONE:
            {
                if( !gotDisplayPicture )
                {
                    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Providing picture at output. Out frame timestamp %1, frame order %2, corrupted %3").
                        arg(decodedFrameCtx->mfxSurface.Data.TimeStamp).arg(decodedFrameCtx->mfxSurface.Data.FrameOrder).arg(decodedFrameCtx->mfxSurface.Data.Corrupted), cl_logDEBUG2 );
                    if( m_prevTimestamp > decodedFrameCtx->mfxSurface.Data.TimeStamp )
                    {
                        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Warning! timestamp decreased by %1. Current 90KHz timestamp %2, previous %3. Ignoring frame...").
                            arg(QString::number(m_prevTimestamp - decodedFrameCtx->mfxSurface.Data.TimeStamp)).
                            arg(QString::number(decodedFrameCtx->mfxSurface.Data.TimeStamp)).
                            arg(QString::number(m_prevTimestamp)),
                            cl_logINFO );
                        m_prevTimestamp = decodedFrameCtx->mfxSurface.Data.TimeStamp;
                        //ignoring frame
                        //break;
                    }
                    m_prevTimestamp = decodedFrameCtx->mfxSurface.Data.TimeStamp;

                    //outFrame = decodedFrame
                    if( outFrame )
                    {
                        saveToAVFrame(
                            outFrame->data(),
                            decodedFrameCtx,
                            true );
                        m_lastAVFrame = *outFrame;
                        (*outFrame)->flags |= QnAbstractMediaData::MediaFlags_HWDecodingUsed;
                    }
                    gotDisplayPicture = true;
                }
                else
                {
                    //decoder returned second picture for same input. Enqueue? We must be sure that we will not come into dead-lock waiting
                        //for some frame to become free before decoding and holding frame(s) in this decoded picture queue
                    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Warning! Got second picture from Intel media decoder during single decoding step! Ignoring..."), cl_logWARNING );
                }
                break;
            }

            case MFX_ERR_MORE_DATA:
                break;

            case MFX_ERR_MORE_SURFACE:
                //provide more surface space and try again...
                continue;

            case MFX_WRN_DEVICE_BUSY:
                ::Sleep( MS_TO_WAIT_FOR_DEVICE );
                ++decodingTry;
                continue;

            case MFX_ERR_DEVICE_LOST:
            case MFX_ERR_DEVICE_FAILED:
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
            {
                //limiting recursion depth
                if( m_recursionDepth > 0 )
                {
                    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Recursion detected!"), cl_logERROR );
                    return false;
                }
                NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Critical error occured during Quicksync decode session %1").
                    arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
                //draining decoded frames from decoder
                mfxBitstream emptyBuffer;
                memset( &emptyBuffer, 0, sizeof(emptyBuffer) );
                //NOTE There can be more than one frame in decoder buffer. It will be good to drain them all...
                ++m_recursionDepth;
                const bool decoderDrainResult = decode( &emptyBuffer, outFrame );
                closeMFXSession();
                m_state = notInitialized;
                //the bitstream pointer is at first bit of new sequence header (mediasdk-man, p.11)
                const bool newSequenceFrameDecodeResult = decode( inputStream, outFrame );
                --m_recursionDepth;
                //if no frame from new sequence, returning drained frame
                return newSequenceFrameDecodeResult || decoderDrainResult;
            }
        }

        if( inputStream->DataLength > 0 )
        {
            //QuickSync processes data by single NALU?
            if( inputStream->DataOffset == prevInputStreamDataOffset && inputStream->DataOffset > 0 )
            {
                //Quick Sync does not eat access_unit_delimiter at the end of a coded frame
                m_totalBytesDropped += inputStream->DataLength;
                NX_LOG( QString::fromLatin1("Warning! Intel media decoder left %1 bytes of data (status %2). Dropping these bytes... Total bytes dropped %3").
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
#else   //USE_ASYNC_IMPL
//NOTE async implementation can increase maximum number of simultaneously decoded streams
bool QuickSyncVideoDecoder::decode( mfxBitstream* const inputStream, QSharedPointer<CLVideoDecoderOutput>* const outFrame )
{
    const bool isSeqHeaderPresent = readSequenceHeader( inputStream ) == MFX_ERR_NONE;
    if( m_reinitializeNeeded && isSeqHeaderPresent )
    {
        //have to wait for a sequence header and I-frame before reinitializing decoder
        closeMFXSession();
        m_reinitializeNeeded = false;
    }
    if( m_state < decoding && !init( MFX_CODEC_AVC, inputStream ) )
        return false;
    //if( (m_state < decoding) && !init( MFX_CODEC_AVC, inputStream ) )
    //    return false;

    AsyncOperationContext* frameToDisplay = NULL;
    checkAsyncOperations( &frameToDisplay );

    if( frameToDisplay )
    {
        //providing picture to output
        frameToDisplay->state = iisOutForDisplay;
        saveToAVFrame( outFrame->data(), frameToDisplay->outPicture );
        m_lastAVFrame = *outFrame;
        _InterlockedDecrement16( (short*)&frameToDisplay->outPicture->mfxSurface.Data.Locked );
        //removing operation frameToDisplay
        for( std::vector<AsyncOperationContext>::iterator
            it = m_currentOperations.begin();
            it != m_currentOperations.end();
            ++it )
        {
            if( frameToDisplay == &*it )
            {
                m_currentOperations.erase( it );
                break;
            }
        }
    }

    //starting new operation
    unsigned int prevInputStreamDataOffset = inputStream->DataOffset;
    for( int decodingTry = 0;
        decodingTry < MAX_ENCODE_ASYNC_CALL_TRIES;
         )
    {
        AsyncOperationContext opCtx;
        for( ;; )
        {
            opCtx.workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
            if( !opCtx.workingSurface.data() )
            {
                NX_LOG( QString::fromLatin1("Can't find unused surface for decoding! Waiting for some async operation to complete..."), cl_logDEBUG1 );
                //waiting for surface to become free
                ::Sleep( ASYNC_OPERATION_WAIT_MS );
                AsyncOperationContext* dummyFrameToDisplay = NULL;
                checkAsyncOperations( &dummyFrameToDisplay ); //checking, whether some async operation has completed
                continue;
            }
            NX_LOG( QString::fromLatin1("Got unused surface for decoding. Proceeding with async operation"), cl_logDEBUG1 );
            break;
        }
        if( inputStream->DataLength == 0 )
            NX_LOG( QString::fromLatin1("Draining decoded frames with NULL input"), cl_logDEBUG1 );

        mfxStatus opStatus = m_decoder->DecodeFrameAsync(
            (inputStream && inputStream->DataLength > 0) ? inputStream : NULL,
            &opCtx.workingSurface->mfxSurface,
            &opCtx.decodedFrameMfxSurf,
            &opCtx.syncPoint );
        if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
            m_decoder->GetVideoParam( &m_srcStreamParam );  //updating stored video-stream parameters
        if( opStatus > MFX_ERR_NONE )
            opStatus = MFX_ERR_NONE;   //ignoring warnings
        if( opStatus == MFX_ERR_NONE && opCtx.syncPoint )
        {
            //assuming, decoder took warkingSurface. New operation started
            opCtx.state = issDecodeInProgress;
            m_currentOperations.push_back( opCtx );
            if( opCtx.workingSurface.data() )
                _InterlockedIncrement16( (short*)&opCtx.workingSurface->mfxSurface.Data.Locked );  //marking workingSurface as used
                //opCtx.workingSurface->locked = true;
            //NOTE we do not use surface Locked counter here so that not to confuse decoder, providing him surface with non-zero Locked counter
            NX_LOG( QString::fromLatin1("Started new async decode operation (pts %1). There are %2 operations in total").
                arg(inputStream->TimeStamp).arg(m_currentOperations.size()), cl_logDEBUG1 );
        }

        switch( opStatus )
        {
            case MFX_ERR_NONE:
            case MFX_ERR_MORE_DATA:
            case MFX_ERR_MORE_SURFACE:
                break;

            case MFX_WRN_DEVICE_BUSY:
                Sleep( MS_TO_WAIT_FOR_DEVICE );
                ++decodingTry;
                continue;

            case MFX_ERR_DEVICE_LOST:
            case MFX_ERR_DEVICE_FAILED:
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
                //limiting recursion depth
                if( m_recursionDepth > 0 )
                {
                    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Recursion detected!"), cl_logERROR );
                    return false;
                }
                NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Critical error occured during Quicksync decode session %1").
                    arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
                //draining decoded frames from decoder
                mfxBitstream emptyBuffer;
                memset( &emptyBuffer, 0, sizeof(emptyBuffer) );
                //NOTE There can be more than one frame in decoder buffer. It will be good to drain them all...
                ++m_recursionDepth;
                const bool decoderDrainResult = decode( &emptyBuffer, outFrame );
                closeMFXSession();
                m_state = notInitialized;
                //the bitstream pointer is at first bit of new sequence header (mediasdk-man, p.11)
                const bool newSequenceFrameDecodeResult = decode( inputStream, outFrame );
                --m_recursionDepth;
                //if no frame from new sequence, returning drained frame
                return newSequenceFrameDecodeResult || decoderDrainResult;
        }

        if( inputStream->DataLength > 0 )
        {
            //QuickSync processes data by single NALU?
            if( inputStream->DataOffset == prevInputStreamDataOffset && inputStream->DataOffset > 0 )
            {
                //Quick Sync does not eat access_unit_delimiter at the end of a coded frame
                m_totalBytesDropped += inputStream->DataLength;
                NX_LOG( QString::fromLatin1("Warning! Intel media decoder left %1 bytes of data (status %2). Dropping these bytes... Total bytes dropped %3").
                    arg(QString::number(inputStream->DataLength)).
                    arg(QString::number(opStatus)).
                    arg(QString::number(m_totalBytesDropped)),
                    cl_logINFO );
                //decoder didn't consume any data
                return frameToDisplay != NULL;
            }
            prevInputStreamDataOffset = inputStream->DataOffset;
            continue;
        }

        break;
    }

    return frameToDisplay != NULL;
}
#endif  //USE_ASYNC_IMPL

#ifndef XVBA_TEST
//!Not implemented yet
void QuickSyncVideoDecoder::setLightCpuMode( DecodeMode /*val*/ )
{
    //TODO/IMPL
}
#endif

//!Implementation of QnAbstractVideoDecoder::getWidth
int QuickSyncVideoDecoder::getWidth() const
{
    return !m_outPictureSize.isValid()
        ? m_srcStreamParam.mfx.FrameInfo.Width
        : m_outPictureSize.width();
}

//!Implementation of QnAbstractVideoDecoder::getHeight
int QuickSyncVideoDecoder::getHeight() const
{
    return !m_outPictureSize.isValid()
        ? m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom)
        : m_outPictureSize.height();
}

//!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
QSize QuickSyncVideoDecoder::getOriginalPictureSize() const
{
    return QSize(
        m_srcStreamParam.mfx.FrameInfo.Width,
        m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom) );
}

#ifndef XVBA_TEST
//!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
double QuickSyncVideoDecoder::getSampleAspectRatio() const
{
    return m_srcStreamParam.mfx.FrameInfo.AspectRatioH > 0
        ? ((double)m_srcStreamParam.mfx.FrameInfo.AspectRatioW / m_srcStreamParam.mfx.FrameInfo.AspectRatioH)
        : 1.0;
}

//!Reset decoder. Used for seek
void QuickSyncVideoDecoder::resetDecoder( QnConstCompressedVideoDataPtr /*data*/ )
{
    if( m_decodingInitialized )
        m_decoder->Reset( &m_srcStreamParam );
}
#endif

const AVFrame* QuickSyncVideoDecoder::lastFrame() const
{
    QSharedPointer<SurfaceContext> mfxSurface;
    //taking most recent unscaled frame
        //NOTE surfaces from m_decoderSurfacePool are only used internally in this class

    std::unique_lock<std::mutex> lk( m_mutex ); //should do synchronization with no mutex, but i'm too lazy

    for( size_t i = 0; i < m_decoderSurfacePool.size(); ++i )
    {
        const QSharedPointer<SurfaceContext>& surfaceCtx = m_decoderSurfacePool.at(i);
        if( (surfaceCtx->mfxSurface.Data.Locked == 0) ||
            (surfaceCtx->didWeLockedSurface && (surfaceCtx->syncCtx.externalRefCounter.load() == 1) && (surfaceCtx->mfxSurface.Data.Locked == 1)) )
        {
            //surface is not locked by decoder
            if( !mfxSurface || mfxSurface->mfxSurface.Data.TimeStamp < surfaceCtx->mfxSurface.Data.TimeStamp )
                mfxSurface = surfaceCtx;    //taking frame with greatest timestamp
        }
    }

    if( !mfxSurface )
        return nullptr; //TODO/IMPL assert?

    if( !m_lastFrameInSysMem )
        m_lastFrameInSysMem = new CLVideoDecoderOutput();
    const_cast<QuickSyncVideoDecoder*>(this)->saveToAVFrame( m_lastFrameInSysMem, mfxSurface, false, true );
    return m_lastFrameInSysMem;
}

static unsigned int PIC_WIDTH_STEP = 16;
static unsigned int PIC_HEIGHT_STEP = 32;

void QuickSyncVideoDecoder::setOutPictureSize( const QSize& outSizeVal )
{
    QSize outSize = outSizeVal;

#ifndef USE_SYSMEM_SURFACE
    static const unsigned int MIN_PICTURE_WIDTH = 32;
    static const unsigned int MIN_PICTURE_HEIGHT = 32;

    const QSize& originalSize = getOriginalPictureSize();

    outSize.setWidth( std::min<size_t>( std::max<int>( outSize.width(), MIN_PICTURE_WIDTH ), originalSize.width() ) );
    outSize.setHeight( std::min<size_t>( std::max<int>( outSize.height(), MIN_PICTURE_HEIGHT ), originalSize.height() ) );

    //in case we will disable scaling, restoring original cropping
    m_frameCropTopActual = m_originalFrameCropTop;
    m_frameCropBottomActual = m_originalFrameCropBottom;

    const double originalAspectRatio = (double)originalSize.width() / originalSize.height();

    NX_ASSERT( originalSize.width() > 0 && originalSize.height() > 0 );

    //TODO/IMPL preserving original aspect ratio
    const double requestedWidthCompression = (double)outSize.width() / originalSize.width();
    const double requestedHeightCompression = (double)outSize.height() / originalSize.height();
    const double compressionRatioToApply = (requestedWidthCompression + requestedHeightCompression) / 2;

    //QSize alignedNewPicSize = QSize( ALIGN16_DOWN(outSize.width()), ALIGN32_DOWN(outSize.height()) );

    const QSizeF newSizeF( originalSize.width() * compressionRatioToApply, originalSize.height() * compressionRatioToApply );
    //we have to align but keep aspect ratio by using cropping
    QSize alignedNewPicSize( ALIGN16_DOWN((unsigned int)newSizeF.width()), ALIGN32_DOWN((unsigned int)newSizeF.height()) );

    //in case we reached zero by aligning, checking once again
    alignedNewPicSize.setWidth( std::max<int>( alignedNewPicSize.width(), MIN_PICTURE_WIDTH ) );
    alignedNewPicSize.setHeight( std::max<int>( alignedNewPicSize.height(), MIN_PICTURE_HEIGHT ) );

    //determining cropping necessary to preserve aspect ratio
    bool canDecreaseAspectRatio = true;
    unsigned int newCropBottom = 0;
    for( ;; )
    {
        double currentAspectRatio = (double)alignedNewPicSize.width() / (alignedNewPicSize.height() - newCropBottom);
        if( currentAspectRatio > originalAspectRatio )
        {
            if( !canDecreaseAspectRatio )
                break;  //done
            //decreasing new aspect ratio
            alignedNewPicSize.setWidth( alignedNewPicSize.width() - PIC_WIDTH_STEP );
        }
        else if( currentAspectRatio == originalAspectRatio )
        {
            break;
        }
        else    //currentAspectRatio < originalAspectRatio
        {
            canDecreaseAspectRatio = false;
            //increasing new aspect ratio by cropping bottom
            ++newCropBottom;
            if( newCropBottom == PIC_HEIGHT_STEP )
            {
                newCropBottom = 0;
                alignedNewPicSize.setHeight( alignedNewPicSize.height() - PIC_HEIGHT_STEP );
            }
        }
    }

    if( alignedNewPicSize == m_outPictureSize )
        return;

    m_outPictureSize = alignedNewPicSize;
    NX_LOG( QString::fromLatin1("Quicksync decoder. Setting up scaling to %1x%2").arg(m_outPictureSize.width()).arg(m_outPictureSize.height()), cl_logINFO );
    if( m_outPictureSize == getOriginalPictureSize() )
        m_outPictureSize = QSize();

    m_frameCropTopActual = 0;
    m_frameCropBottomActual = newCropBottom;

#ifdef SCALE_WITH_MFX
    if( m_state < decoding )
        return;

    if( m_processor.get() )
    {
        //have to reinitialize whole session because after destruction of processor decoder does not want to work properly
        m_reinitializeNeeded = true;
    }
    else
    {
        //just creating processor
        initProcessor();
    }
#else
    if( m_scaledFramePool.empty() )
        initializeScaleSurfacePool();
#endif
#endif  //USE_SYSMEM_SURFACE
}

QnAbstractPictureDataRef::PicStorageType QuickSyncVideoDecoder::targetMemoryType() const
{
    return QnAbstractPictureDataRef::pstD3DSurface;
}

#ifndef XVBA_TEST
unsigned int QuickSyncVideoDecoder::getDecoderCaps() const
{
#ifdef USE_SYSMEM_SURFACE
    return QnAbstractVideoDecoder::tracksDecodedPictureBuffer;
#else
    return QnAbstractVideoDecoder::decodedPictureScaling | QnAbstractVideoDecoder::tracksDecodedPictureBuffer;
#endif
}

void QuickSyncVideoDecoder::setSpeed( float newValue )
{
    if( !m_eventReceiver )
        return;
    m_speed = newValue;
    m_eventReceiver->streamParamsChanged( this, *this );
}

bool QuickSyncVideoDecoder::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::framePictureWidth:
            *value = m_srcStreamParam.mfx.FrameInfo.Width;
            return true;
        case DecoderParameter::framePictureHeight:
            *value = m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom);
            return true;
        case DecoderParameter::framePictureSize:
            *value = m_srcStreamParam.mfx.FrameInfo.Width * (m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom));
            return true;
        case DecoderParameter::fps:
            *value = m_sourceStreamFps;
            return true;
        case DecoderParameter::pixelsPerSecond:
            *value = m_sourceStreamFps * m_srcStreamParam.mfx.FrameInfo.Width * (m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom));
            return true;
        case DecoderParameter::videoMemoryUsage:
            *value = (qlonglong)m_decoderSurfaceSizeInBytes * m_decoderSurfacePool.size() + (qlonglong)m_scaledSurfaceSizeInBytes * m_scaledFramePool.size();
            return true;
        case DecoderParameter::speed:
            *value = m_speed;
            return true;
        default:
            return false;
    }
}
#endif

bool QuickSyncVideoDecoder::readSequenceHeader( const QnCompressedVideoDataPtr data, mfxVideoParam* const streamParams )
{
    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    //inputStream->TimeStamp = av_rescale_q( data->timestamp, SRC_DATA_TIMESTAMP_RESOLUTION, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION );
    inputStream.TimeStamp = data->timestamp;
#ifndef XVBA_TEST
    if( isH264SeqHeaderInExtraData(data) )
    {
        std::basic_string<mfxU8> seqHeader;
        readH264SeqHeaderFromExtraData( data, &seqHeader );

        inputStream.Data = const_cast<mfxU8*>(seqHeader.data());
        inputStream.DataLength = seqHeader.size();
        inputStream.MaxLength = seqHeader.size();
        return readSequenceHeader( &inputStream, streamParams ) == MFX_ERR_NONE;
    }
#endif

#ifndef XVBA_TEST
    inputStream.Data = reinterpret_cast<mfxU8*>(data->data.data());
#else
    inputStream.Data = reinterpret_cast<mfxU8*>(const_cast<char*>(data->data.data()));
#endif
    inputStream.DataLength = data->data.size();
    inputStream.MaxLength = data->data.size();
    //inputStream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

    return readSequenceHeader( &inputStream, streamParams ) == MFX_ERR_NONE;
}

bool QuickSyncVideoDecoder::init( mfxU32 codecID, mfxBitstream* seqHeader )
{
    DWORD t1 = GetTickCount();

    readSequenceHeader( seqHeader );
    m_frameCropTopActual = m_originalFrameCropTop;  //in case we do not need VPP
    m_frameCropBottomActual = m_originalFrameCropBottom;

    //DWORD t2 = GetTickCount();
    if( !m_mfxSessionEstablished && !initMfxSession() )
        return false;

    //DWORD t3 = GetTickCount();
    //NX_LOG( QString::fromLatin1("mark1 %1 ms. %2").arg(t3-t2).arg(t3), cl_logERROR );

    if( !m_decodingInitialized && !initDecoder( codecID, seqHeader ) )
        return false;

    //initializing fps with value, read from stream. Later we will calculate fps based on frame pts
    m_sourceStreamFps = m_srcStreamParam.mfx.FrameInfo.FrameRateExtD > 0
        ? m_srcStreamParam.mfx.FrameInfo.FrameRateExtN / (float)m_srcStreamParam.mfx.FrameInfo.FrameRateExtD
        : 15;   //TODO

    //DWORD t4 = GetTickCount();
    //NX_LOG( QString::fromLatin1("mark2 %1 ms. %2").arg(t4-t3).arg(t4), cl_logERROR );

#ifndef USE_SYSMEM_SURFACE
#ifdef SCALE_WITH_MFX
    if( processingNeeded() && !m_processor.get() && !initProcessor() )
        return false;
#else
    initializeScaleSurfacePool();

    if( processingNeeded() && (m_outPictureSize.width() > getOriginalPictureSize().width() || m_outPictureSize.height() > getOriginalPictureSize().height()) )
        setOutPictureSize( getOriginalPictureSize() );  //it is no use to scale decoded picture to the size greater than original size
#endif
#endif  //USE_SYSMEM_SURFACE

    //DWORD t5 = GetTickCount();
    //NX_LOG( QString::fromLatin1("mark3 %1 ms. Total %2 ms. %3").arg(t5-t4).arg(t5-t1).arg(t5), cl_logERROR );

#ifdef USE_OPENCL
    initOpenCL();
#endif

    m_state = decoding;

#ifndef XVBA_TEST
    m_pluginUsageWatcher->decoderCreated( this );
#endif

    m_totalInputFrames = 0;
    m_totalOutputFrames = 0;

    NX_LOG( QString::fromLatin1("Quicksync decoder initialization took %1 ms").arg(GetTickCount()-t1), cl_logDEBUG1 );

    return true;
}

bool QuickSyncVideoDecoder::initMfxSession()
{
    mfxStatus status = MFX_ERR_NONE;
    //opening media session
    if( m_parentSession )
    {
        status = m_parentSession->CloneSession( &m_mfxSession.getInternalSession() );
        if( status != MFX_ERR_NONE )
        {
            NX_LOG( QString::fromLatin1("Failed to clone parent Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            return false;
        }

        status = MFXJoinSession( *m_parentSession, m_mfxSession );
        if( status != MFX_ERR_NONE )
        {
            NX_LOG( QString::fromLatin1("Failed to join Intel media SDK session with parent session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            //m_mfxSession.Close();
            //return false;
        }
    }
    else
    {
        mfxVersion version;
        memset( &version, 0, sizeof(version) );
        version.Major = 1;
        status = m_mfxSession.Init( MFX_IMPL_AUTO_ANY, &version );
        if( status != MFX_ERR_NONE )
        {
            NX_LOG( QString::fromLatin1("Failed to create Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            m_mfxSession.Close();
            return false;
        }
    }

    m_mfxSessionEstablished = true;

#ifndef USE_SYSMEM_SURFACE
    status = m_mfxSession.SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_d3dFrameAllocator.d3d9DeviceManager() );
    if( status != MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to set IDirect3DDeviceManager9 pointer to MFX session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
#endif

    status = m_mfxSession.SetBufferAllocator( &m_bufAllocator );
    if( status != MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to set buffer allocator to Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
    status = m_mfxSession.SetFrameAllocator( &m_frameAllocator );
    if( status != MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to set frame allocator to Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }

    return true;
}

bool QuickSyncVideoDecoder::initDecoder( mfxU32 codecID, mfxBitstream* seqHeader )
{
    NX_ASSERT( m_mfxSessionEstablished );

    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Initializing decoder..."), cl_logDEBUG1 );

    if( !m_decoder.get() )
    {
        //initializing decoder
        m_decoder.reset( new MFXVideoDECODE( m_mfxSession ) );
    }
    //NX_LOG( QString::fromLatin1("mark01 %1 ms").arg(GetTickCount()), cl_logERROR );

#if defined(WRITE_INPUT_STREAM_TO_FILE_2)
    m_inputStreamFile.write( (const char*)seqHeader->Data, seqHeader->DataLength );
#endif

    m_state = decodingHeader;

    mfxBitstream tmp;
    if( m_cachedSequenceHeader.size() > 0 )
    {
        m_cachedSequenceHeader.append( seqHeader->Data, seqHeader->MaxLength );
        memset( &tmp, 0, sizeof(tmp) );
        tmp.Data = const_cast<mfxU8*>(m_cachedSequenceHeader.data());
        tmp.MaxLength = m_cachedSequenceHeader.size();
        tmp.DataLength = m_cachedSequenceHeader.size();
        seqHeader = &tmp;
    }

    //NX_LOG( QString::fromLatin1("mark01.4 %1 ms").arg(GetTickCount()), cl_logERROR );

    m_srcStreamParam.mfx.CodecId = codecID;
#if 1
    mfxStatus status = readSequenceHeader( seqHeader, &m_srcStreamParam );
#else
    mfxStatus status = m_decoder->DecodeHeader( seqHeader, &m_srcStreamParam );
#endif
    //NX_LOG( QString::fromLatin1("mark01.5 %1 ms").arg(GetTickCount()), cl_logERROR );
    switch( status )
    {
        case MFX_ERR_NONE:
            break;

        case MFX_ERR_MORE_DATA:
            NX_LOG( QString::fromLatin1("Need more data to parse sequence header"), cl_logWARNING );
            if( m_cachedSequenceHeader.empty() )
                m_cachedSequenceHeader.append( seqHeader->Data, seqHeader->MaxLength );
            return false;

        default:
            if( status > MFX_ERR_NONE )
            {
                //warning
                if( status == MFX_WRN_PARTIAL_ACCELERATION )
                {
                    NX_LOG( QString::fromLatin1("Warning. Intel Media SDK can't use hardware acceleration for decoding"), cl_logWARNING );
                    m_hardwareAccelerationEnabled = false;
                }
                break;
            }
            NX_LOG( QString::fromLatin1("Failed to decode stream header during Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            m_decoder.reset();
            return false;
    }
    m_cachedSequenceHeader.clear();

    //NX_LOG( QString::fromLatin1("mark01.6 %1 ms").arg(GetTickCount()), cl_logERROR );

    NX_LOG( QString::fromLatin1("Successfully decoded stream header with Intel Media decoder! Picture resolution %1x%2, stream profile/level: %3/%4").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)),
        cl_logINFO );

    //checking, whether hardware acceleration enabled
    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession.QueryIMPL( &actualImplUsed );
    m_hardwareAccelerationEnabled = actualImplUsed != MFX_IMPL_SOFTWARE;

    //NX_LOG( QString::fromLatin1("mark01.7 %1 ms").arg(GetTickCount()), cl_logERROR );

    m_srcStreamParam.AsyncDepth = 1;
    m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_srcStreamParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
#ifdef USE_SYSMEM_SURFACE
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#else
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#endif

    //status = m_decoder->Query( &m_srcStreamParam, &m_srcStreamParam );

    mfxFrameAllocRequest decodingAllocRequest;
    memset( &decodingAllocRequest, 0, sizeof(decodingAllocRequest) );
#ifndef OWN_NUM_FRAMES_CALCULATION
    status = m_decoder->QueryIOSurf( &m_srcStreamParam, &decodingAllocRequest );
    if( status < MFX_ERR_NONE ) //ignoring warnings
    {
        NX_LOG( QString::fromLatin1("Failed to query surface information for decoding from Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_decoder.reset();
        return false;
    }

#else
    //loading created surfaces
    decodingAllocRequest.Info = m_srcStreamParam.mfx.FrameInfo;
    decodingAllocRequest.NumFrameMin = m_sps->num_ref_frames + 2;   //decoder needs num_ref_frames+1 frames,
    decodingAllocRequest.NumFrameSuggested = decodingAllocRequest.NumFrameMin;
#ifdef USE_SYSMEM_SURFACE
    decodingAllocRequest.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;
#else
    decodingAllocRequest.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
#endif  //USE_SYSMEM_SURFACE
#endif  //OWN_NUM_FRAMES_CALCULATION
    //NX_LOG( QString::fromLatin1("mark01.9 %1 ms").arg(GetTickCount()), cl_logERROR );

    allocateSurfacePool( &m_decoderSurfacePool, &decodingAllocRequest, &m_decodingAllocResponse );
    //NX_LOG( QString::fromLatin1("mark02 %1 ms").arg(GetTickCount()), cl_logERROR );

    //decoder uses allocator to create necessary surfaces
    m_srcStreamParam.mfx.TimeStampCalc = MFX_TIMESTAMPCALC_UNKNOWN;
    status = m_decoder->Init( &m_srcStreamParam );
    if( status < MFX_ERR_NONE )  //ignoring warnings
    {
        NX_LOG( QString::fromLatin1("Failed to initialize Intel media SDK decoder. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        if( m_decodingAllocResponse.NumFrameActual > 0 )
            m_frameAllocator._free( &m_decodingAllocResponse );
        m_decoderSurfacePool.clear();
        m_decoder.reset();
        return false;
    }
    //NX_LOG( QString::fromLatin1("mark03. clock %1 ms").arg(GetTickCount()), cl_logERROR );

    //calculating surface size in bytes
    if( !m_decoderSurfacePool.empty() )
    {
        m_frameAllocator.lock( m_decoderSurfacePool[0]->mfxSurface.Data.MemId, &m_decoderSurfacePool[0]->mfxSurface.Data );
        m_decoderSurfaceSizeInBytes = (m_decoderSurfacePool[0]->mfxSurface.Data.Pitch * (m_decoderSurfacePool[0]->mfxSurface.Info.Height - m_frameCropBottomActual)) / 2 * 3;
        m_frameAllocator.unlock( m_decoderSurfacePool[0]->mfxSurface.Data.MemId, &m_decoderSurfacePool[0]->mfxSurface.Data );
    }

    NX_LOG( QString::fromLatin1("Intel Media decoder successfully initialized! Picture resolution %1x%2, "
            "stream profile/level: %3/%4. Allocated %5 surfaces in %6 memory...").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)).
        arg(QString::number(m_decoderSurfacePool.size())).
        arg(QString::fromLatin1("video")),
        cl_logINFO );

    //checking, whether hardware acceleration enabled
    m_mfxSession.QueryIMPL( &actualImplUsed );
    m_hardwareAccelerationEnabled = actualImplUsed != MFX_IMPL_SOFTWARE;

    m_decodingInitialized = true;
    return true;
}

#ifdef SCALE_WITH_MFX
bool QuickSyncVideoDecoder::initProcessor()
{
    NX_ASSERT( m_decodingInitialized );

    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Initializing VPP processor..."), cl_logDEBUG1 );

    if( m_outPictureSize == QSize(m_srcStreamParam.mfx.FrameInfo.Width, m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom)) )
    {
        //no need for scaling
        m_frameCropTopActual = m_originalFrameCropTop;
        m_frameCropBottomActual = m_originalFrameCropBottom;
        m_outPictureSize = QSize();
        return true;
    }
    else
    {
        //seems like intel VPP performes cropping...
        m_frameCropTopActual = 0;
        m_frameCropBottomActual = 0;
    }

    m_processor.reset( new MFXVideoVPP( m_mfxSession ) );

    //setting up re-scaling and de-interlace (if apropriate)
    memset( &m_processingParams, 0, sizeof(m_processingParams) );
#ifdef USE_ASYNC_IMPL
    m_processingParams.AsyncDepth = 0;  //TODO
#else
    m_processingParams.AsyncDepth = 1;
#endif
    m_processingParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    m_processingParams.vpp.In = m_srcStreamParam.mfx.FrameInfo;
    m_processingParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    m_processingParams.vpp.In.CropX = 0;
    m_processingParams.vpp.In.CropY = 0;
    m_processingParams.vpp.In.CropW = m_processingParams.vpp.In.Width;
    m_processingParams.vpp.In.CropH = m_processingParams.vpp.In.Height;
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
    //m_processingParams.vpp.Out.CropW = m_srcStreamParam.mfx.FrameInfo.Width;
    //m_processingParams.vpp.Out.CropH = m_srcStreamParam.mfx.FrameInfo.Height;
    m_processingParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_processingParams.vpp.Out.FrameRateExtD = m_processingParams.vpp.In.FrameRateExtD;
    m_processingParams.vpp.Out.FrameRateExtN = m_processingParams.vpp.In.FrameRateExtN;

    mfxFrameAllocRequest request[2];
    memset( request, 0, sizeof(request) );

    mfxStatus status = m_processor->Query( &m_processingParams, &m_processingParams );
    if( status < MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to query VPP. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }
    else if( status > MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Warning from quering VPP. Status %1").arg(mfxStatusCodeToString(status)), cl_logWARNING );
    }

    status = m_processor->QueryIOSurf( &m_processingParams, request );
    if( status < MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to query surface information for VPP from Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }
    else if( status > MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Warning from quering VPP (io surf). Status %1").arg(mfxStatusCodeToString(status)), cl_logWARNING );
    }

    //U00, V00, U01, V01, U10, V10, U11, V11;
    //U00, U01, U10, U11, V00, V01, V10, V11;
    //U00, U10, V00, V10, U01, U11, V01, V11;
    //U00, V00, U01, V01, U10, V10, U11, V11;

    //as input surfaces will use decoder output

    request[1].NumFrameSuggested = std::max<size_t>( request[1].NumFrameSuggested, m_decoderSurfacePool.size() );

    allocateSurfacePool( &m_scaledFramePool, &request[1], &m_vppAllocResponse );
    for( int i = 0; i < m_scaledFramePool.size(); ++i )
        m_scaledFramePool[i]->mfxSurface.Info = m_processingParams.vpp.Out;

    status = m_processor->Init( &m_processingParams );
    if( status != MFX_ERR_NONE )
    {
        NX_LOG( QString::fromLatin1("Failed to initialize Intel media SDK processor. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    //calculating surface size in bytes
    if( !m_scaledFramePool.empty() )
    {
        m_frameAllocator.lock( m_scaledFramePool[0]->mfxSurface.Data.MemId, &m_scaledFramePool[0]->mfxSurface.Data );
        m_scaledSurfaceSizeInBytes = (m_scaledFramePool[0]->mfxSurface.Data.Pitch * (m_scaledFramePool[0]->mfxSurface.Info.Height - m_frameCropBottomActual)) / 2 * 3;
        m_frameAllocator.unlock( m_scaledFramePool[0]->mfxSurface.Data.MemId, &m_scaledFramePool[0]->mfxSurface.Data );
    }

    NX_LOG( QString::fromLatin1("Intel Media processor (scale to %1x%2) successfully initialized!. Allocated %3 surfaces in %4 memory").
        arg(m_outPictureSize.width()).arg(m_outPictureSize.height()).
        arg(m_scaledFramePool.size()).
        arg(QString::fromLatin1("video")),
        cl_logINFO );

    return true;
}
#endif

#ifdef USE_OPENCL
bool QuickSyncVideoDecoder::initOpenCL()
{
    static const size_t MAX_PLATFORMS_NUMBER = 8;
    cl_platform_id platforms[MAX_PLATFORMS_NUMBER];
    memset( platforms, 0, sizeof(platforms) );
    cl_uint numPlatforms = 0;
    m_prevCLStatus = clGetPlatformIDs( MAX_PLATFORMS_NUMBER, platforms, &numPlatforms );
    if( m_prevCLStatus != CL_SUCCESS )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. clGetPlatformIDs failed. %1").arg(prevCLErrorText()), cl_logERROR );
        return false;
    }
    if( numPlatforms == 0 )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. No OpenCL platform IDs found"), cl_logERROR );
        return false;
    }

    //TODO/IMPL select intel platform
    size_t intelCLPlatformIndex = 1;

    //unfortunately, CL_PLATFORM_EXTENSIONS platform info is unavailable on intel

    //char platformExtensionsStr[256];
    //size_t actualPlatformExtensionsStrSize = 0;
    //m_prevCLStatus = clGetPlatformInfo(
    //    platforms[intelCLPlatformIndex],
    //    CL_PLATFORM_EXTENSIONS,
    //    sizeof(platformExtensionsStr),
    //    platformExtensionsStr,
    //    &actualPlatformExtensionsStrSize );
    //platformExtensionsStr[std::min<size_t>(actualPlatformExtensionsStrSize, sizeof(platformExtensionsStr)/sizeof(*platformExtensionsStr))] = 0;
    //if( m_prevCLStatus != CL_SUCCESS )
    //{
    //    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. clGetPlatformInfo failed. %1").arg(prevCLErrorText()), cl_logERROR );
    //    return false;
    //}
    //NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Supported CL extension: %1").arg(QString::fromLatin1(platformExtensionsStr)), cl_logDEBUG1 );

    if( !m_clGetDeviceIDsFromDX9INTEL )
        m_clGetDeviceIDsFromDX9INTEL = (clGetDeviceIDsFromDX9INTEL_fn)clGetExtensionFunctionAddress( "clGetDeviceIDsFromDX9INTEL" );
    if( !m_clCreateFromDX9MediaSurfaceINTEL )
        m_clCreateFromDX9MediaSurfaceINTEL= (clCreateFromDX9MediaSurfaceINTEL_fn)clGetExtensionFunctionAddress( "clCreateFromDX9MediaSurfaceINTEL" );
    //clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddress( "clGetGLContextInfoKHR" );
    if( !m_clGetDeviceIDsFromDX9INTEL || !m_clCreateFromDX9MediaSurfaceINTEL )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Seems like no cl_intel_dx9_media_sharing extension on this platform... %1"), cl_logERROR );
        return false;
    }

    static const size_t MAX_DEVICES_NUMBER = 8;
    cl_device_id clDevices[MAX_DEVICES_NUMBER];
    memset( clDevices, 0, sizeof(clDevices) );
    cl_uint numOfDevices = 0;
    m_prevCLStatus = clGetDeviceIDs(
        platforms[intelCLPlatformIndex],
        CL_DEVICE_TYPE_ALL,
        MAX_DEVICES_NUMBER,
        clDevices,
        &numOfDevices );
    //m_prevCLStatus = clGetDeviceIDsFromDX9INTEL(
    //    platforms[intelCLPlatformIndex],
    //    CL_D3D9EX_DEVICE_INTEL,
    //    m_d3dFrameAllocator.d3d9Device(),
    //    CL_ALL_DEVICES_FOR_DX9_INTEL,   //CL_PREFERRED_DEVICES_FOR_DX9_INTEL,
    //    MAX_DEVICES_NUMBER,
    //    clDevices,
    //    &numOfDevices );
    if( m_prevCLStatus != CL_SUCCESS )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. clGetDeviceIDs failed. %1").arg(prevCLErrorText()), cl_logERROR );
        return false;
    }
    if( numOfDevices == 0 )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. No OpenCL device IDs found"), cl_logERROR );
        return false;
    }

    cl_context_properties properties[] = {
        CL_CONTEXT_PLATFORM,            (cl_context_properties)(platforms[intelCLPlatformIndex]),
        CL_CONTEXT_D3D9EX_DEVICE_INTEL, (cl_context_properties)m_d3dFrameAllocator.d3d9Device(),
        //CL_GL_CONTEXT_KHR,              (cl_context_properties)m_glContext,
        //CL_WGL_HDC_KHR,                 (cl_context_properties)m_glContextDevice,
        0 };
    m_clContext = clCreateContext(
        properties,
        1,
        clDevices,  //TODO/IMPL take appropriate device
        NULL,
        NULL,
        &m_prevCLStatus );
    if( m_prevCLStatus != CL_SUCCESS )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. clCreateContext failed. %1").arg(prevCLErrorText()), cl_logERROR );
        return false;
    }

    //TODO/IMPL
    return true;
}
#endif

void QuickSyncVideoDecoder::allocateSurfacePool(
    SurfacePool* const surfacePool,
    mfxFrameAllocRequest* const allocRequest,
    mfxFrameAllocResponse* const allocResponse )
{
    memset( allocResponse, 0, sizeof(*allocResponse) );
    m_frameAllocator.alloc( allocRequest, allocResponse );
    initializeSurfacePoolFromAllocResponse( surfacePool, *allocResponse );
}

void QuickSyncVideoDecoder::initializeSurfacePoolFromAllocResponse(
    SurfacePool* const surfacePool,
    const mfxFrameAllocResponse& allocResponse )
{
    surfacePool->reserve( surfacePool->size() + allocResponse.NumFrameActual );
    for( int i = 0; i < allocResponse.NumFrameActual; ++i )
    {
        surfacePool->push_back( QSharedPointer<SurfaceContext>( new SurfaceContext() ) );
        memcpy( &surfacePool->back()->mfxSurface.Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
        surfacePool->back()->mfxSurface.Data.MemId = allocResponse.mids[i];
    }
}

void QuickSyncVideoDecoder::closeMFXSession()
{
#ifdef SCALE_WITH_MFX
    m_processor.reset();
#endif

    if( m_decoder.get() )
    {
        m_decoder->Reset( &m_srcStreamParam );
        m_decoder.reset();
        m_decodingInitialized = false;
    }

    if( m_mfxSessionEstablished )
    {
        if( m_parentSession )
            m_mfxSession.DisjoinSession();

        m_mfxSession.Close();
        m_mfxSessionEstablished = false;
    }

    //free surface pool
    for( SurfacePool::iterator
        it = m_decoderSurfacePool.begin();
        it != m_decoderSurfacePool.end();
        ++it )
    {
        ++(*it)->syncCtx.sequence;
        //waiting for the surface to become unused (usage counter must drop to zero)
        while( (*it)->syncCtx.usageCounter > 0 )
            _mm_pause();    //we're in spin loop
    }
    m_decoderSurfacePool.clear();
    if( m_decodingAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_decodingAllocResponse );

    for( SurfacePool::iterator
        it = m_scaledFramePool.begin();
        it != m_scaledFramePool.end();
        ++it )
    {
        ++(*it)->syncCtx.sequence;
        //waiting for the surface to become unused (usage counter must drop to zero)
        while( (*it)->syncCtx.usageCounter > 0 )
            _mm_pause();    //we're in spin loop
    }
    m_scaledFramePool.clear();
    if( m_vppAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_vppAllocResponse );

    m_state = notInitialized;
}

#ifdef USE_ASYNC_IMPL
void QuickSyncVideoDecoder::checkAsyncOperations( AsyncOperationContext** const operationReadyForDisplay )
{
    *operationReadyForDisplay = NULL;
    mfxStatus opStatus = MFX_ERR_NONE;
    unsigned int triesCounter = 0;    //try to repeat operation counter
    for( std::vector<AsyncOperationContext>::iterator
        it = m_currentOperations.begin();
        it != m_currentOperations.end();
        ++triesCounter )
    {
        //iterating existing surfaces:
            //if surface is used, checking syncpoint, is it done?
        switch( it->state )
        {
            case issDecodeInProgress:
                opStatus = m_mfxSession.SyncOperation( it->syncPoint, 0 );
                if( opStatus < MFX_ERR_NONE )
                    break;  //error
                if( opStatus == MFX_WRN_IN_EXECUTION )
                    break;   //to next surface

                if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
                    m_decoder->GetVideoParam( &m_srcStreamParam );  //updating stored video-stream parameters

                if( it->workingSurface.data() )    //working surface is NULL in case of draining decoded frames
                {
                    //marking it->workingSurface as unused (if decoder needs it, it will increase Locked counter)
                    _InterlockedDecrement16( (short*)&it->workingSurface->mfxSurface.Data.Locked );
                    //it->workingSurface->locked = false;
                    it->workingSurface.clear();
                }

                if( opStatus == MFX_ERR_NONE && it->decodedFrameMfxSurf )
                {
                    it->decodedFrame = getSurfaceCtxFromSurface( it->decodedFrameMfxSurf );
                    _InterlockedIncrement16( (short*)&it->decodedFrame->mfxSurface.Data.Locked );
                    it->decodedFrameMfxSurf = NULL;

                    //operation produced picture, moving state...
                    it->prevOperationResult = opStatus;
                    if( m_processor.get() )
                    {
                        it->state = issWaitingVPP;
                    }
                    else
                    {
                        it->state = issReadyForPresentation;
                        it->outPicture = it->decodedFrame;
                    }

                    //futher operation processing immediately
                    continue;
                }
                break;

            case issWaitingVPP:
                //if surface contains decoded frame,
                    //searching for unused vpp_out surface
                it->outPicture = findUnlockedSurface( &m_scaledFramePool );
                if( !it->outPicture.data() )
                {
                    //NOTE it is normal, that we cannot find unused surface for vpp, since we give out vpp_out surfaces to the caller and it is unspecified,
                        //when it will release vpp_out surface. Because of this we need all this states & transitions
                    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Cannot start VPP of frame (pts %1), since there is no unused vpp_out surface").
                        arg(it->decodedFrame->mfxSurface.Data.TimeStamp), cl_logDEBUG1 );
                    break;  //to next surface
                }

                //if found,
                    //starting VPP
                opStatus = m_processor->RunFrameVPPAsync( &it->decodedFrame->mfxSurface, &it->outPicture->mfxSurface, NULL, &it->syncPoint );
                if( opStatus < MFX_ERR_NONE )
                {
                    it->outPicture.clear();
                    break;  //error
                }
                it->state = issVPPInProgress;
                it->outPicture->mfxSurface.Data.TimeStamp = it->decodedFrame->mfxSurface.Data.TimeStamp;
                //marking it->outPicture surface as used
                _InterlockedIncrement16( (short*)&it->outPicture->mfxSurface.Data.Locked );

                NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. starting frame (pts %1) post-processing. Result %2").
                    arg(it->decodedFrame->mfxSurface.Data.TimeStamp).arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
                break;

            case issVPPInProgress:
                opStatus = m_mfxSession.SyncOperation( it->syncPoint, 0 );
                if( opStatus < MFX_ERR_NONE )
                    break;  //error
                if( opStatus == MFX_WRN_IN_EXECUTION )
                    break;   //to next surface

                //moving surface state
                it->state = issReadyForPresentation;
                it->prevOperationResult = opStatus;

                //we don't need it->decodedFrame anymore
                _InterlockedDecrement16( (short*)&it->decodedFrame->mfxSurface.Data.Locked );
                it->decodedFrame.clear();
#if 0
                // Marking it->outPicture surface as unused.
                markSurfaceAsUnused( it->outPicture );
                // Locking surface.
                _InterlockedIncrement16( (short*)&it->outPicture->Data.Locked );
#endif // 0
                continue;   //processing same operation once more

            case issReadyForPresentation:
                //if surface VPP is done, providing vpp_out surface to output (only one surface per decode call)
                    //have to find issReadyForPresentation surface with min pts
                if( !*operationReadyForDisplay || (*operationReadyForDisplay)->outPicture->mfxSurface.Data.TimeStamp > it->outPicture->mfxSurface.Data.TimeStamp )
                    *operationReadyForDisplay = &*it;
                break;

            //case issUnused:
            //{
            //    if( it->surface->Data.Locked > 0 )
            //        break;  //surface is still locked by decoder

            //    if( inputStream->DataLength == 0 )
            //        break;  //no data
            //    if( inputStream->DataOffset == prevInputStreamDataOffset && inputStream->DataOffset > 0 )
            //    {
            //        //Quick Sync does not eat access_unit_delimiter at the end of a coded frame
            //        m_totalBytesDropped += inputStream->DataLength;
            //        NX_LOG( QString::fromLatin1("Warning! Intel media decoder left %1 bytes of data (status %2). Dropping these bytes... Total bytes dropped %3").
            //            arg(QString::number(inputStream->DataLength)).
            //            arg(QString::number(opStatus)).
            //            arg(QString::number(m_totalBytesDropped)),
            //            cl_logINFO );
            //        //decoder didn't consume any data, ignoring left data...
            //        inputStream->DataLength = 0;
            //        break;
            //    }

            //    prevInputStreamDataOffset = inputStream->DataOffset;
            //    //and finding one unused surface ready to decode to
            //        //and starting async decoding
            //    opStatus = m_decoder->DecodeFrameAsync( &inputStream, &it->surface, &it->decodingOutSurface, &it->syncPoint );
            //    it->pts = data->timestamp;
            //    if( it->surface->Data.Locked > 0 )  //decoder locked surface
            //        it->state = issDecodeInProgress;    //moving state even in case of error (non-critical)

            //    if( opStatus < MFX_ERR_NONE )
            //        break;  //error

            //    if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
            //    {
            //        //updating stored video-stream parameters
            //        m_decoder->GetVideoParam( &m_srcStreamParam );
            //    }
            //    //moving state
            //    it->state = issDecodeInProgress;
            //    break;
            //}

            default:
                break;
        }

        if( opStatus < MFX_ERR_NONE )
        {
            switch( opStatus )
            {
                case MFX_ERR_MORE_DATA:
                case MFX_ERR_MORE_SURFACE:
                    //provide more surface space and try again...
                    break;

                case MFX_WRN_DEVICE_BUSY:
                    ::Sleep( MS_TO_WAIT_FOR_DEVICE );
                    if( triesCounter < MAX_ENCODE_ASYNC_CALL_TRIES )
                        continue;   //repeating call
                    break;

                case MFX_ERR_DEVICE_LOST:
                case MFX_ERR_DEVICE_FAILED:
                case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
                default:
                    NX_LOG( QString::fromLatin1("Critical error occured during Quicksync decode session %1").arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
                    resetMfxSession();
                    *operationReadyForDisplay = NULL;
                    return;
            }
        }

        ++it;
        triesCounter = 0;
    }
}
#endif

QSharedPointer<QuickSyncVideoDecoder::SurfaceContext> QuickSyncVideoDecoder::findUnlockedSurface( SurfacePool* const surfacePool )
{
    if( surfacePool == &m_decoderSurfacePool )
    {
        int unusedSurfaces = 0;
        for( size_t i = 0; i < surfacePool->size(); ++i )
            if( surfacePool->at(i)->mfxSurface.Data.Locked == 0 )
                ++unusedSurfaces;
        NX_LOG( QString::fromLatin1("There are %1 unused decoder surfaces").arg(unusedSurfaces), cl_logDEBUG2 );
    }

    //performing 2 tries because external reference to surface can be unlocked at any time
    for( int tryNumber = 0; tryNumber < 2; ++tryNumber )
    {
        for( size_t i = 0; i < surfacePool->size(); ++i )
        {
            SurfaceContext& surfaceCtx = *surfacePool->at(i);
            if( surfaceCtx.mfxSurface.Data.Locked == 0 )
                return surfacePool->at(i);
            if( surfaceCtx.didWeLockedSurface && (surfaceCtx.syncCtx.externalRefCounter.load() == 1)   //only we are holding lock to the surface...
                && (surfaceCtx.mfxSurface.Data.Locked == 1) )           //surface is not locked by decoder
            {
                --surfaceCtx.syncCtx.externalRefCounter;
                _InterlockedDecrement16( (short*)&surfaceCtx.mfxSurface.Data.Locked );
                surfaceCtx.didWeLockedSurface = false;
                return surfacePool->at(i);
            }
        }

        if( tryNumber == 1 )
        {
            //looks like all surfaces are locked by media sdk. This is bad...
            return QSharedPointer<SurfaceContext>();
        }

        //could not find unused surface: forcing unlock of used surface...
        for( size_t i = 0; i < surfacePool->size(); ++i )
        {
            SurfaceContext& surfaceCtx = *surfacePool->at(i);

            //searching for a surface that is not locked by decoder. While we here counters can only decrease
            if( !(surfaceCtx.didWeLockedSurface
                  && surfaceCtx.syncCtx.externalRefCounter.load() > 0
                  && surfaceCtx.mfxSurface.Data.Locked == 1) )
                continue;
            //surface can be unlocked in any time

            //incrementing sequence counter so that current references become invalid
            ++surfaceCtx.syncCtx.sequence;
            const DWORD t1 = GetTickCount();
            if( surfaceCtx.syncCtx.usageCounter > 0 )
            {
                NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Invalidated frame (0x%1) refs, waiting for refs to be released... (externalRefCounter = %2, Locked = %3)").
                    arg((size_t)&surfaceCtx.syncCtx, 0, 16).arg(surfaceCtx.syncCtx.externalRefCounter).arg(surfaceCtx.mfxSurface.Data.Locked), cl_logDEBUG1 );
            }
            //waiting for the surface to become unused (usage counter must drop to zero)
            while( surfaceCtx.syncCtx.usageCounter > 0 )
                _mm_pause();    //we're in spin loop

            //by this moment all external refs to the surface are invalidated (since sequence is modified) and are not allowed to use surface contexts
            --surfaceCtx.syncCtx.externalRefCounter;
            _InterlockedDecrement16( (short*)&surfaceCtx.mfxSurface.Data.Locked );
            surfaceCtx.didWeLockedSurface = false;
            NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Cancelled frame (0x%1) rendering (externalRefCounter = %2, Locked = %3). It took %4 ms").
                arg((size_t)&surfaceCtx.syncCtx, 0, 16).arg(surfaceCtx.syncCtx.externalRefCounter).arg(surfaceCtx.mfxSurface.Data.Locked).arg(GetTickCount()-t1), cl_logDEBUG1 );
            return surfacePool->at(i);
        }

        //we did not find anything. May be some surface became free while we were searching?
    }

    return QSharedPointer<SurfaceContext>();
}

bool QuickSyncVideoDecoder::processingNeeded() const
{
    return m_outPictureSize.isValid();
}

inline void fastmemcpy_sse4( __m128i* dst, __m128i* src, size_t sz )
{
    const __m128i* const src_end = src + sz / sizeof(__m128i);
    while( src < src_end )
    {
         __m128i x1 = _mm_stream_load_si128( src );
         __m128i x2 = _mm_stream_load_si128( src+1 );
         __m128i x3 = _mm_stream_load_si128( src+2 );
         __m128i x4 = _mm_stream_load_si128( src+3 );

         src += 4;

         _mm_store_si128( dst, x1 );
         _mm_store_si128( dst+1, x2 );
         _mm_store_si128( dst+2, x3 );
         _mm_store_si128( dst+3, x4 );

         dst += 4;
    }
}

//inline void fastmemcpy_sse4_buffered( __m128i* dst, __m128i* src, size_t sz )
//{
//    static const size_t TMP_BUF_SIZE = 1*1024 / sizeof(__m128i);
//    __m128i tmpBuf[TMP_BUF_SIZE];
//
//    const __m128i* const src_end = src + sz / sizeof(__m128i);
//    while( src < src_end )
//    {
//        //reading to tmp buffer
//        const size_t bytesToCopy = std::min<size_t>( (src_end - src)*sizeof(*src), sizeof(tmpBuf) );
//        fastmemcpy_sse4( tmpBuf, src, bytesToCopy );
//        src += bytesToCopy / sizeof(*src);
//
//        //storing
//        const __m128i* const tmpBufEnd = tmpBuf + bytesToCopy / sizeof(*tmpBuf);
//        for( __m128i* srcTmp = tmpBuf; srcTmp < tmpBufEnd; )
//        {
//            __m128i x1 = *srcTmp;
//            __m128i x2 = *(srcTmp+1);
//            __m128i x3 = *(srcTmp+2);
//            __m128i x4 = *(srcTmp+3);
//            srcTmp += 4;
//
//             _mm_store_si128( dst, x1 );
//             _mm_store_si128( dst+1, x2 );
//             _mm_store_si128( dst+2, x3 );
//             _mm_store_si128( dst+3, x4 );
//             dst += 4;
//        }
//    }
//}

static void loadAndDeinterleaveUVPlane(
    __m128i* nv12UVPlane,
    size_t nv12UVPlaneSize,
    __m128i* yv12UPlane,
    __m128i* yv12VPlane )
{
    static const size_t TMP_BUF_SIZE = 4*1024 / sizeof(__m128i);
    __m128i tmpBuf[TMP_BUF_SIZE];

    //static const __m128i MASK_LO = { 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff };
    static const __m128i MASK_HI = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };
    static const __m128i AND_MASK = MASK_HI;
    static const __m128i SHIFT_8 = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const __m128i* const nv12UVPlaneEnd = nv12UVPlane + nv12UVPlaneSize / sizeof(__m128i);
    while( nv12UVPlane < nv12UVPlaneEnd )
    {
        //reading to tmp buffer
        const size_t bytesToCopy = std::min<size_t>( (nv12UVPlaneEnd - nv12UVPlane)*sizeof(*nv12UVPlane), sizeof(tmpBuf) );
        fastmemcpy_sse4( tmpBuf, nv12UVPlane, bytesToCopy );
        nv12UVPlane += bytesToCopy / sizeof(*nv12UVPlane);

        //converting
        const __m128i* const tmpBufEnd = tmpBuf + bytesToCopy / sizeof(*tmpBuf);
        for( __m128i* nv12UVPlaneTmp = tmpBuf; nv12UVPlaneTmp < tmpBufEnd; )
        {
            __m128i x1 = *nv12UVPlaneTmp;
            __m128i x2 = *(nv12UVPlaneTmp+1);
            __m128i x3 = *(nv12UVPlaneTmp+2);
            __m128i x4 = *(nv12UVPlaneTmp+3);

            __m128i x5 = _mm_packus_epi16( _mm_and_si128( x1, AND_MASK ), _mm_and_si128( x2, AND_MASK ) );   //U
            __m128i x6 = _mm_packus_epi16( _mm_srl_epi16( x1, SHIFT_8 ), _mm_srl_epi16( x2, SHIFT_8 ) );   //V
            _mm_store_si128( yv12UPlane, x5 );
            _mm_store_si128( yv12VPlane, x6 );

            x5 = _mm_packus_epi16( _mm_and_si128( x3, AND_MASK ), _mm_and_si128( x4, AND_MASK ) );   //U
            x6 = _mm_packus_epi16( _mm_srl_epi16( x3, SHIFT_8 ), _mm_srl_epi16( x4, SHIFT_8 ) );   //V
            _mm_store_si128( yv12UPlane+1, x5 );
            _mm_store_si128( yv12VPlane+1, x6 );

            yv12UPlane += 2;
            yv12VPlane += 2;

            nv12UVPlaneTmp += 4;
        }
    }
}

void QuickSyncVideoDecoder::saveToAVFrame(
    CLVideoDecoderOutput* outFrame,
    const QSharedPointer<SurfaceContext>& decodedFrameCtx,
    bool returnD3DSurface,
    bool convertNV12ToYV12 )
{
    const mfxFrameSurface1* const decodedFrame = &decodedFrameCtx->mfxSurface;

#if defined(USE_OPENCL)
    mfxHDL handle = NULL;
    if( m_clContext && m_frameAllocator.gethdl( decodedFrameCtx->mfxSurface.Data.MemId, &handle ) == MFX_ERR_NONE )
    {
        cl_mem clFrame = m_clCreateFromDX9MediaSurfaceINTEL(
           m_clContext,
           CL_MEM_READ_ONLY,
           static_cast<IDirect3DSurface9*>(handle),
           NULL,
           0,
           &m_prevCLStatus );
        if( m_prevCLStatus != CL_SUCCESS )
        {
            NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. clCreateFromDX9MediaSurfaceINTEL failed. %1").arg(prevCLErrorText()), cl_logERROR );
        }
    }
#endif

    if( returnD3DSurface )
    {
        //locking surface
        _InterlockedIncrement16( (short*)&decodedFrameCtx->mfxSurface.Data.Locked );
        ++decodedFrameCtx->syncCtx.externalRefCounter;
        decodedFrameCtx->didWeLockedSurface = true;
        outFrame->picData = QSharedPointer<QnAbstractPictureDataRef>(
            new QuicksyncDXVAPictureData(
                &m_frameAllocator,
                decodedFrameCtx,
                //QRect(0, m_frameCropTopActual, decodedFrame->Info.Width, decodedFrame->Info.Height - (m_frameCropTopActual + m_frameCropBottomActual))
                QRect( 0, 0, getWidth(), getHeight() ),
                this ) );
    }
    else
    {
        //outFrame = decodedFrame
        m_frameAllocator.lock( decodedFrameCtx->mfxSurface.Data.MemId, &decodedFrameCtx->mfxSurface.Data );

        //if( !outFrame->isExternalData() )
        {
            const QRect& cropRect = QRect( 0, 0, getWidth(), getHeight() );

            //copying frame data if needed
            outFrame->reallocate(
                decodedFrame->Data.Pitch,
                decodedFrame->Info.Height - (m_frameCropTopActual + m_frameCropBottomActual),
                !convertNV12ToYV12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P,
                decodedFrame->Data.Pitch );

    #ifdef USE_SYSMEM_SURFACE
            memcpy(
                outFrame->data[0],
                decodedFrame->Data.Y + (m_frameCropTopActual * decodedFrame->Data.Pitch),
                decodedFrame->Data.Pitch * (decodedFrame->Info.Height - (m_frameCropTopActual+m_frameCropBottomActual)) );
    #else
            fastmemcpy_sse4(
                (__m128i*)outFrame->data[0],
                (__m128i*)(decodedFrame->Data.Y + (m_frameCropTopActual * decodedFrame->Data.Pitch)),
                decodedFrame->Data.Pitch * (decodedFrame->Info.Height - (m_frameCropTopActual+m_frameCropBottomActual)) );
    #endif

            //const DWORD t1 = GetTickCount();
            //for( int i = 0; i < 5000; ++i )
            //{
            if( !convertNV12ToYV12 )
                fastmemcpy_sse4(
                    (__m128i*)outFrame->data[1],
                    (__m128i*)(decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch)),
                    decodedFrame->Data.Pitch * (decodedFrame->Info.Height - m_frameCropBottomActual) / 2 );
            else
                loadAndDeinterleaveUVPlane(
                    (__m128i*)(decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch)),
                    decodedFrame->Data.Pitch * (decodedFrame->Info.Height - m_frameCropBottomActual) / 2,
                    (__m128i*)outFrame->data[1],
                    (__m128i*)outFrame->data[2] );
            //}
            //const DWORD t2 = GetTickCount();
            //qDebug() << "!!!!!!!!!!!!!"<<(t2 - t1)<<"!!!!!!!!!!!!!1";
        }
        //else
        //{
        //    outFrame->data[0] = decodedFrame->Data.Y + m_frameCropTopActual * decodedFrame->Data.Pitch;
        //    outFrame->data[1] = decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
        //    outFrame->data[2] = decodedFrame->Data.V + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
        //}
        outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
        if( !convertNV12ToYV12 )
        {
            outFrame->linesize[1] = decodedFrame->Data.Pitch;       //uv_stride
            outFrame->linesize[2] = 0;
        }
        else
        {
            outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //u_stride
            outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;   //v_stride
        }
    }

    const QSize& originalPictureSize = getOriginalPictureSize();
    outFrame->width = originalPictureSize.width();
    outFrame->height = originalPictureSize.height();
    outFrame->format = !convertNV12ToYV12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
    //outFrame->key_frame = decodedFrame->Data.
    //outFrame->pts = av_rescale_q( decodedFrame->Data.TimeStamp, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION, SRC_DATA_TIMESTAMP_RESOLUTION );
    outFrame->pts = decodedFrame->Data.TimeStamp;
    outFrame->pkt_dts = decodedFrame->Data.TimeStamp;

    outFrame->display_picture_number = decodedFrame->Data.FrameOrder;
    outFrame->interlaced_frame =
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF ||
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF ||
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_REPEATED;

    if( !returnD3DSurface )
        m_frameAllocator.unlock( decodedFrameCtx->mfxSurface.Data.MemId, &decodedFrameCtx->mfxSurface.Data );
}

mfxStatus QuickSyncVideoDecoder::readSequenceHeader( mfxBitstream* const inputStream, mfxVideoParam* const streamParams )
{
    std::unique_ptr<SPSUnit> sps;
    std::unique_ptr<PPSUnit> pps;
    bool spsFound = false;
    bool ppsFound = false;

    if( inputStream->DataLength <= 4 )
        return MFX_ERR_MORE_DATA;

    bool requirePicTimingSEI = false;
    const quint8* dataStart = reinterpret_cast<const quint8*>(inputStream->Data + inputStream->DataOffset);
    const quint8* dataEnd = dataStart + inputStream->DataLength;
    for( const quint8
         *curNalu = NALUnit::findNextNAL( dataStart, dataEnd ),
         *nextNalu = NULL;
         curNalu < dataEnd;
         curNalu = nextNalu )
    {
        nextNalu = NALUnit::findNextNAL( curNalu, dataEnd );
        NX_ASSERT( nextNalu > curNalu );
        switch( *curNalu & 0x1f )
        {
            case nuSPS:
            {
                sps.reset( new SPSUnit() );
                sps->decodeBuffer( curNalu, nextNalu );
                sps->deserialize();

                //reading frame cropping settings
                const unsigned int SubHeightC = sps->chroma_format_idc == 1 ? 2 : 1;
                const unsigned int CropUnitY = (sps->chroma_format_idc == 0)
                    ? (2 - sps->frame_mbs_only_flag)
                    : (SubHeightC * (2 - sps->frame_mbs_only_flag));
                m_originalFrameCropTop = CropUnitY * sps->frame_crop_top_offset;
                m_originalFrameCropBottom = CropUnitY * sps->frame_crop_bottom_offset;

                if( m_sps.get() )
                    if( m_sps->seq_parameter_set_id != sps->seq_parameter_set_id )
                    {
                        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. SPS id changed from %1 to %2. Reinitializing...").
                            arg(m_sps->seq_parameter_set_id).arg(sps->seq_parameter_set_id), cl_logDEBUG1 );
                        m_reinitializeNeeded = true;
                    }
                    else if( (m_sps->pic_width_in_mbs != sps->pic_width_in_mbs) || (m_sps->pic_height_in_map_units != sps->pic_height_in_map_units) )
                    {
                        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Picture size in SPS has changed from %1.%2 to %3.%4. Reinitializing...").
                            arg(m_sps->getWidth()).arg(m_sps->getHeight()).arg(sps->getWidth()).arg(sps->getHeight()), cl_logDEBUG1 );
                        m_reinitializeNeeded = true;
                    }

                m_sps = sps;
                spsFound = true;

                if( streamParams )
                {
                    streamParams->mfx.CodecLevel = m_sps->level_idc;
                    streamParams->mfx.CodecProfile = m_sps->profile_idc;
                    streamParams->mfx.FrameInfo.Width = m_sps->getWidth();
                    streamParams->mfx.FrameInfo.Height = m_sps->pic_height_in_map_units * 16;
                    if( m_sps->aspect_ratio_info_present_flag )
                    {
                        if( m_sps->aspect_ratio_idc == h264::AspectRatio::Extended_SAR )
                        {
                            streamParams->mfx.FrameInfo.AspectRatioW = m_sps->sar_width;
                            streamParams->mfx.FrameInfo.AspectRatioH = m_sps->sar_height;
                        }
                        else
                        {
                            unsigned int arw = 0;
                            unsigned int arh = 0;
                            h264::AspectRatio::decode( m_sps->aspect_ratio_idc, &arw, &arh );
                            streamParams->mfx.FrameInfo.AspectRatioW = arw;
                            streamParams->mfx.FrameInfo.AspectRatioH = arh;
                        }
                    }
                    else
                    {
                        //streamParams->mfx.FrameInfo.AspectRatioW = m_sps->getWidth();
                        //streamParams->mfx.FrameInfo.AspectRatioH = m_sps->getHeight();
                    }
                    streamParams->mfx.FrameInfo.ChromaFormat = m_sps->chroma_format_idc;
                    streamParams->mfx.FrameInfo.CropH = streamParams->mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom);
                    streamParams->mfx.FrameInfo.CropW = m_sps->getWidth();
                    streamParams->mfx.FrameInfo.CropX = 0;
                    streamParams->mfx.FrameInfo.CropY = m_originalFrameCropTop;
                    streamParams->mfx.FrameInfo.FourCC = MAKEFOURCC('H', '2', '6', '4');
                    streamParams->mfx.FrameInfo.FrameRateExtN = 30000; //TODO/IMPL fps is not required information for decoder to work
                    streamParams->mfx.FrameInfo.FrameRateExtD = 1001;
                    //if( m_sps->timing_info_present_flag )
                    //{
                    //    streamParams->mfx.FrameInfo.FrameRateExtD = ;
                    //    streamParams->mfx.FrameInfo.FrameRateExtN = ;
                    //}
                    //else
                    //{
                    //}
                    //if( !m_sps->pic_struct_present_flag )
                        streamParams->mfx.FrameInfo.PicStruct = 0;
                    //else
                    //    requirePicTimingSEI = true;
                }
                break;
            }

            case nuPPS:
            {
                pps.reset( new PPSUnit() );
                pps->decodeBuffer( curNalu, nextNalu );
                pps->deserialize();

                m_pps = pps;
                ppsFound = true;
                break;
            }

            case nuSEI:
            {
                if( !m_sps.get() || !requirePicTimingSEI )
                    break;

                SEIUnit sei;
                sei.decodeBuffer( curNalu, nextNalu );
                sei.deserialize( *m_sps.get(), m_sps->nal_hrd_parameters_present_flag || m_sps->vcl_hrd_parameters_present_flag ? 1 : 0 );

                if( sei.m_processedMessages.find(h264::SEIType::pic_timing) != sei.m_processedMessages.end() )
                {
                    if( streamParams )
                        streamParams->mfx.FrameInfo.PicStruct = sei.pic_struct;
                    requirePicTimingSEI = false;
                }
                break;
            }

            default:
                break;
        }
    }

    if( spsFound && ppsFound && !requirePicTimingSEI )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Found and parsed sequence header"), cl_logDEBUG1 );
        return MFX_ERR_NONE;
    }

    return MFX_ERR_MORE_DATA;
}

size_t QuickSyncVideoDecoder::estimateSurfaceMemoryUsage() const
{
    if( !m_sps.get() )
        return 0;

    return ((m_sps->num_ref_frames + 2) * (m_sps->getWidth() * m_sps->getHeight())  //decoder surface
#ifndef USE_SYSMEM_SURFACE
            + SCALED_SURFACE_COUNT * m_sps->getWidth() * m_sps->getHeight() //surfaces used for scaling
#endif
            )* 3 / 2;   //decoder uses NV12 format. Every pixel is 12 bits in size
}

void QuickSyncVideoDecoder::setInitializationMode( bool val )
{
    m_initializationMode = val;
}

QString QuickSyncVideoDecoder::mfxStatusCodeToString( mfxStatus status ) const
{
    switch( status )
    {
        case MFX_ERR_NONE:
            return QString::fromLatin1("no error");

        /* reserved for unexpected errors */
        case MFX_ERR_UNKNOWN:
            return QString::fromLatin1("unknown error");

        /* error codes <0 */
        case MFX_ERR_NULL_PTR:
            return QString::fromLatin1("null pointer");
        case MFX_ERR_UNSUPPORTED:
            return QString::fromLatin1("undeveloped feature");
        case MFX_ERR_MEMORY_ALLOC:
            return QString::fromLatin1("failed to allocate memory");
        case MFX_ERR_NOT_ENOUGH_BUFFER:
            return QString::fromLatin1("insufficient buffer at input");
        case MFX_ERR_INVALID_HANDLE:
            return QString::fromLatin1("invalid handle");
        case MFX_ERR_LOCK_MEMORY:
            return QString::fromLatin1("failed to lock the memory block");
        case MFX_ERR_NOT_INITIALIZED:
            return QString::fromLatin1("member function called before initialization");
        case MFX_ERR_NOT_FOUND:
            return QString::fromLatin1("the specified object is not found");
        case MFX_ERR_MORE_DATA:
            return QString::fromLatin1("expect more data at input");
        case MFX_ERR_MORE_SURFACE:
            return QString::fromLatin1("expect more surface at output");
        case MFX_ERR_ABORTED:
            return QString::fromLatin1("operation aborted");
        case MFX_ERR_DEVICE_LOST:
            return QString::fromLatin1("lose the HW acceleration device");
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            return QString::fromLatin1("incompatible video parameters");
        case MFX_ERR_INVALID_VIDEO_PARAM:
            return QString::fromLatin1("invalid video parameters");
        case MFX_ERR_UNDEFINED_BEHAVIOR:
            return QString::fromLatin1("undefined behavior");
        case MFX_ERR_DEVICE_FAILED:
            return QString::fromLatin1("device operation failure");
        case MFX_ERR_MORE_BITSTREAM:
            return QString::fromLatin1("expect more bitstream buffers at output");

        /* warnings >0 */
        case MFX_WRN_IN_EXECUTION:
            return QString::fromLatin1("the previous asynchrous operation is in execution");
        case MFX_WRN_DEVICE_BUSY:
            return QString::fromLatin1("the HW acceleration device is busy");
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            return QString::fromLatin1("the video parameters are changed during decoding");
        case MFX_WRN_PARTIAL_ACCELERATION:
            return QString::fromLatin1("SW is used");
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
            return QString::fromLatin1("incompatible video parameters");
        case MFX_WRN_VALUE_NOT_CHANGED:
            return QString::fromLatin1("the value is saturated based on its valid range");
        case MFX_WRN_OUT_OF_RANGE:
            return QString::fromLatin1("the value is out of valid range");

        /* threading statuses */
        case MFX_TASK_WORKING:
            return QString::fromLatin1("there is some more work to do");
        case MFX_TASK_BUSY:
            return QString::fromLatin1("task is waiting for resources");
        default:
            return QString::fromLatin1("unknown error");
    }
}

QSharedPointer<QuickSyncVideoDecoder::SurfaceContext> QuickSyncVideoDecoder::getSurfaceCtxFromSurface( mfxFrameSurface1* const surf ) const
{
    for( SurfacePool::size_type i = 0;
        i < m_decoderSurfacePool.size();
        ++i )
    {
        if( &m_decoderSurfacePool[i]->mfxSurface == surf )
            return m_decoderSurfacePool[i];
    }

    for( SurfacePool::size_type i = 0;
        i < m_scaledFramePool.size();
        ++i )
    {
        if( &m_scaledFramePool[i]->mfxSurface == surf )
            return m_scaledFramePool[i];
    }

    return QSharedPointer<SurfaceContext>();
}

void QuickSyncVideoDecoder::resetMfxSession()
{
    NX_LOG( QString::fromLatin1("Resetting Quicksync decode session..."), cl_logERROR );

    m_decoder->Reset( &m_srcStreamParam );
#ifdef SCALE_WITH_MFX
    if( m_processor.get() )
        m_processor->Reset( &m_processingParams );
#endif

#ifdef USE_ASYNC_IMPL
    //resetting all operations, freeing surfaces...
    for( std::vector<AsyncOperationContext>::iterator
        it = m_currentOperations.begin();
        it != m_currentOperations.end();
        ++it )
    {
        if( it->workingSurface.data() )
        {
            _InterlockedDecrement16( (short*)&it->workingSurface->mfxSurface.Data.Locked );
            it->workingSurface.clear();
        }
        if( it->decodedFrame.data() )
        {
            _InterlockedDecrement16( (short*)&it->decodedFrame->mfxSurface.Data.Locked );
            it->decodedFrame.clear();
        }
        if( it->outPicture.data() )
        {
            _InterlockedDecrement16( (short*)&it->outPicture->mfxSurface.Data.Locked );
            it->outPicture.clear();
        }
    }

    m_currentOperations.clear();
#endif
}

#ifdef USE_OPENCL
QString QuickSyncVideoDecoder::prevCLErrorText() const
{
    switch( m_prevCLStatus )
    {
        case CL_SUCCESS:
            return QString::fromLatin1("CL_SUCCESS");
        case CL_DEVICE_NOT_FOUND:
            return QString::fromLatin1("CL_DEVICE_NOT_FOUND");
        case CL_DEVICE_NOT_AVAILABLE:
            return QString::fromLatin1("CL_DEVICE_NOT_AVAILABLE");
        case CL_COMPILER_NOT_AVAILABLE:
            return QString::fromLatin1("CL_COMPILER_NOT_AVAILABLE");
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:
            return QString::fromLatin1("CL_MEM_OBJECT_ALLOCATION_FAILURE");
        case CL_OUT_OF_RESOURCES:
            return QString::fromLatin1("CL_OUT_OF_RESOURCES");
        case CL_OUT_OF_HOST_MEMORY:
            return QString::fromLatin1("CL_OUT_OF_HOST_MEMORY");
        case CL_PROFILING_INFO_NOT_AVAILABLE:
            return QString::fromLatin1("CL_PROFILING_INFO_NOT_AVAILABLE");
        case CL_MEM_COPY_OVERLAP:
            return QString::fromLatin1("CL_MEM_COPY_OVERLAP");
        case CL_IMAGE_FORMAT_MISMATCH:
            return QString::fromLatin1("CL_IMAGE_FORMAT_MISMATCH");
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            return QString::fromLatin1("CL_IMAGE_FORMAT_NOT_SUPPORTED");
        case CL_BUILD_PROGRAM_FAILURE:
            return QString::fromLatin1("CL_BUILD_PROGRAM_FAILURE");
        case CL_MAP_FAILURE:
            return QString::fromLatin1("CL_MAP_FAILURE");
        case CL_MISALIGNED_SUB_BUFFER_OFFSET:
            return QString::fromLatin1("CL_MISALIGNED_SUB_BUFFER_OFFSET");
        case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
            return QString::fromLatin1("CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST");
        case CL_INVALID_VALUE:
            return QString::fromLatin1("CL_INVALID_VALUE");
        case CL_INVALID_DEVICE_TYPE:
            return QString::fromLatin1("CL_INVALID_DEVICE_TYPE");
        case CL_INVALID_PLATFORM:
            return QString::fromLatin1("CL_INVALID_PLATFORM");
        case CL_INVALID_DEVICE:
            return QString::fromLatin1("CL_INVALID_DEVICE");
        case CL_INVALID_CONTEXT:
            return QString::fromLatin1("CL_INVALID_CONTEXT");
        case CL_INVALID_QUEUE_PROPERTIES:
            return QString::fromLatin1("CL_INVALID_QUEUE_PROPERTIES");
        case CL_INVALID_COMMAND_QUEUE:
            return QString::fromLatin1("CL_INVALID_COMMAND_QUEUE");
        case CL_INVALID_HOST_PTR:
            return QString::fromLatin1("CL_INVALID_HOST_PTR");
        case CL_INVALID_MEM_OBJECT:
            return QString::fromLatin1("CL_INVALID_MEM_OBJECT");
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
            return QString::fromLatin1("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR");
        case CL_INVALID_IMAGE_SIZE:
            return QString::fromLatin1("CL_INVALID_IMAGE_SIZE");
        case CL_INVALID_SAMPLER:
            return QString::fromLatin1("CL_INVALID_SAMPLER");
        case CL_INVALID_BINARY:
            return QString::fromLatin1("CL_INVALID_BINARY");
        case CL_INVALID_BUILD_OPTIONS:
            return QString::fromLatin1("CL_INVALID_BUILD_OPTIONS");
        case CL_INVALID_PROGRAM:
            return QString::fromLatin1("CL_INVALID_PROGRAM");
        case CL_INVALID_PROGRAM_EXECUTABLE:
            return QString::fromLatin1("CL_INVALID_PROGRAM_EXECUTABLE");
        case CL_INVALID_KERNEL_NAME:
            return QString::fromLatin1("CL_INVALID_KERNEL_NAME");
        case CL_INVALID_KERNEL_DEFINITION:
            return QString::fromLatin1("CL_INVALID_KERNEL_DEFINITION");
        case CL_INVALID_KERNEL:
            return QString::fromLatin1("CL_INVALID_KERNEL");
        case CL_INVALID_ARG_INDEX:
            return QString::fromLatin1("CL_INVALID_ARG_INDEX");
        case CL_INVALID_ARG_VALUE:
            return QString::fromLatin1("CL_INVALID_ARG_VALUE");
        case CL_INVALID_ARG_SIZE:
            return QString::fromLatin1("CL_INVALID_ARG_SIZE");
        case CL_INVALID_KERNEL_ARGS:
            return QString::fromLatin1("CL_INVALID_KERNEL_ARGS");
        case CL_INVALID_WORK_DIMENSION:
            return QString::fromLatin1("CL_INVALID_WORK_DIMENSION");
        case CL_INVALID_WORK_GROUP_SIZE:
            return QString::fromLatin1("CL_INVALID_WORK_GROUP_SIZE");
        case CL_INVALID_WORK_ITEM_SIZE:
            return QString::fromLatin1("CL_INVALID_WORK_ITEM_SIZE");
        case CL_INVALID_GLOBAL_OFFSET:
            return QString::fromLatin1("CL_INVALID_GLOBAL_OFFSET");
        case CL_INVALID_EVENT_WAIT_LIST:
            return QString::fromLatin1("CL_INVALID_EVENT_WAIT_LIST");
        case CL_INVALID_EVENT:
            return QString::fromLatin1("CL_INVALID_EVENT");
        case CL_INVALID_OPERATION:
            return QString::fromLatin1("CL_INVALID_OPERATION");
        case CL_INVALID_GL_OBJECT:
            return QString::fromLatin1("CL_INVALID_GL_OBJECT");
        case CL_INVALID_BUFFER_SIZE:
            return QString::fromLatin1("CL_INVALID_BUFFER_SIZE");
        case CL_INVALID_MIP_LEVEL:
            return QString::fromLatin1("CL_INVALID_MIP_LEVEL");
        case CL_INVALID_GLOBAL_WORK_SIZE:
            return QString::fromLatin1("CL_INVALID_GLOBAL_WORK_SIZE");
        case CL_INVALID_PROPERTY:
            return QString::fromLatin1("CL_INVALID_PROPERTY");
        default:
            return QString();
    }
}
#endif

#ifndef SCALE_WITH_MFX
#ifndef USE_SYSMEM_SURFACE
void QuickSyncVideoDecoder::initializeScaleSurfacePool()
{
    mfxFrameAllocRequest request;
    memset( &request, 0, sizeof(request) );
    //request.NumFrameSuggested = SCALED_SURFACE_COUNT;  //m_decoderSurfacePool.size();
    request.NumFrameSuggested = m_decoderSurfacePool.size();
    request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    //creating surfaces with original picture size so that we do not have to re-create them upon output picture size change
    //request.Info.Width = getOriginalPictureSize().width();
    //request.Info.Height = getOriginalPictureSize().height();
    request.Info.Width = m_srcStreamParam.mfx.FrameInfo.Width;
    request.Info.Height = m_srcStreamParam.mfx.FrameInfo.Height;

    allocateSurfacePool( &m_scaledFramePool, &request, &m_vppAllocResponse );
    for( size_t i = 0; i < m_scaledFramePool.size(); ++i )
        m_scaledFramePool[i]->mfxSurface.Info = request.Info; // m_processingParams.vpp.Out;

    m_scaledSurfaceSizeInBytes = request.Info.Width * request.Info.Height * 3 / 2;
}

mfxStatus QuickSyncVideoDecoder::scaleFrame( const mfxFrameSurface1& from, mfxFrameSurface1* const to )
{
    HRESULT res = D3D_OK;

    //TODO/IMPL should check this before reporting whether we support scaling or not...
    //res = m_d3dFrameAllocator.d3d9()->CheckDeviceFormatConversion(
    //    m_d3dFrameAllocator.adapterNumber(),
    //    D3DDEVTYPE_HAL,
    //    (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
    //    (D3DFORMAT)MAKEFOURCC('N','V','1','2') );

    //if( res != D3D_OK )
    //{
    //    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Scaling. CheckDeviceFormatConversion failed. %1").
    //        arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
    //    return MFX_ERR_UNKNOWN;
    //}

    IDirect3DSurface9* surfFrom = NULL;
    mfxStatus mfxRes = m_d3dFrameAllocator.gethdl( from.Data.MemId, (mfxHDL*)&surfFrom );
    if( mfxRes )
        return mfxRes;
    IDirect3DSurface9* surfTo = NULL;
    mfxRes = m_d3dFrameAllocator.gethdl( to->Data.MemId, (mfxHDL*)&surfTo );
    if( mfxRes )
        return mfxRes;

    D3DSURFACE_DESC surfFromDesc;
    memset( &surfFromDesc, 0, sizeof(surfFromDesc) );
    res = surfFrom->GetDesc( &surfFromDesc );
    if( res != D3D_OK )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Scaling. Failed to get src surface desc. %1").
            arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
        return MFX_ERR_UNKNOWN;
    }

    RECT srcRect;
    memset( &srcRect, 0, sizeof(srcRect) );
    srcRect.top = m_originalFrameCropTop;
    srcRect.right = surfFromDesc.Width;
    srcRect.bottom = surfFromDesc.Height - (m_originalFrameCropTop + m_originalFrameCropBottom);
    RECT dstRect;
    memset( &dstRect, 0, sizeof(dstRect) );
    dstRect.right = m_outPictureSize.width();
    dstRect.bottom = m_outPictureSize.height();

    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Scaling decoded frame to %1x%2...").
        arg(m_outPictureSize.width()).arg(m_outPictureSize.height()), cl_logDEBUG2 );

    //const DWORD t1 = GetTickCount();
    IDirect3DDevice9* d3d9Device = NULL;
    for( ;; )
    {
        res = m_d3dFrameAllocator.d3d9DeviceManager()->LockDevice( m_d3dFrameAllocator.d3DeviceHandle(), &d3d9Device, TRUE );
        if( res == DXVA2_E_NEW_VIDEO_DEVICE )
        {
            if( !m_d3dFrameAllocator.reopenDeviceHandle() )
                break;
        }
        else
        {
            break;
        }
    }
    if( res != D3D_OK )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Scaling. Failed to lock d3d device. %1").arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
        return MFX_ERR_UNKNOWN;
    }

    //const DWORD t2 = GetTickCount();

    res = d3d9Device->StretchRect( surfFrom, &srcRect, surfTo, &dstRect, D3DTEXF_LINEAR );
    d3d9Device->Release();
    m_d3dFrameAllocator.d3d9DeviceManager()->UnlockDevice( m_d3dFrameAllocator.d3DeviceHandle(), FALSE );

    if( res != D3D_OK )
    {
        NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Scaling. Failed to scale frame. %1").arg(QString::fromWCharArray(DXGetErrorDescription(res))), cl_logERROR );
        return MFX_ERR_UNKNOWN;
    }

    to->Data.TimeStamp = from.Data.TimeStamp;

    //const DWORD t3 = GetTickCount();

    //NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. Frame has been successfully scaled to %1x%2. It took %3 ms (%4 ms to locking)").
    //    arg(m_outPictureSize.width()).arg(m_outPictureSize.height()).arg(t3-t1).arg(t2-t1), cl_logDEBUG1 );

    return MFX_ERR_NONE;
}
#endif
#endif

bool QuickSyncVideoDecoder::isH264SeqHeaderInExtraData( const QnConstCompressedVideoDataPtr data ) const
{
    return data->context && data->context->ctx() && data->context->ctx()->extradata_size >= 7 && data->context->ctx()->extradata[0] == 1;
}

bool QuickSyncVideoDecoder::readH264SeqHeaderFromExtraData(
    const QnConstCompressedVideoDataPtr data,
    std::basic_string<mfxU8>* const seqHeader )
{
    const unsigned char* p = data->context->ctx()->extradata;

    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    // prefix is unit len
    //const int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;
    /* sps and pps in the avcC always have length coded with 2 bytes,
     * so put a fake nal_length_size = 2 while parsing them */
    //int nal_length_size = 2;

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for( int i = 0; i < cnt; i++ )
    {
        const int nalsize = AV_RB16(p);
        p += 2; //skipping nalusize
        if( nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata) )
            break;
        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for( int i = 0; i < cnt; ++i )
    {
        const int nalsize = AV_RB16(p);
        p += 2;
        if( nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata) )
            break;

        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }

    m_nalLengthSize = (data->context->ctx()->extradata[4] & 0x03) + 1;

    return true;
}

mfxStatus QuickSyncVideoDecoder::doDecodingStep( mfxBitstream* const inputStream, mfxFrameSurface1** decodedFrame )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    QSharedPointer<SurfaceContext> workingSurface;
    for( ;; )
    {
        workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
#ifdef SCALE_WITH_MFX
        if( !workingSurface.data() && !m_processor.get() )
#else
        if( !workingSurface.data() && !processingNeeded() )
#endif
        {
            //TODO/IMPL force take surface with oldest picture
            NX_LOG( QString::fromLatin1("No empty frame for decoding. Waiting for a frame to get free..."), cl_logERROR );
            //waiting for a surface to get free
            ::Sleep( MS_TO_WAIT_FOR_DEVICE );
            continue;
        }
        break;
    }
    NX_ASSERT( workingSurface.data() );
    NX_ASSERT( workingSurface->mfxSurface.Data.Locked == 0 );

#ifdef WRITE_INPUT_STREAM_TO_FILE_2
    const int dataOffsetBak = inputStream->DataOffset;
#endif

    //workingSurface->mfxSurface.Data.TimeStamp = inputStream ? inputStream->TimeStamp : 0;
    mfxStatus opStatus = m_decoder->DecodeFrameAsync(
        (inputStream && inputStream->DataLength > 0) ? inputStream : NULL,
        &workingSurface->mfxSurface,
        decodedFrame,
        &m_syncPoint );

#ifdef WRITE_INPUT_STREAM_TO_FILE_2
    if( inputStream->DataOffset - dataOffsetBak > 0 )
        m_inputStreamFile.write( (const char*)inputStream->Data + dataOffsetBak, inputStream->DataOffset - dataOffsetBak );
#endif
    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. starting frame (pts %1) decoding. Result %2").
        arg(inputStream->TimeStamp).arg(mfxStatusCodeToString(opStatus)),
        (opStatus == MFX_ERR_NONE || opStatus == MFX_ERR_MORE_DATA) ? cl_logDEBUG2 : cl_logDEBUG1 );

    return opStatus;
}

mfxStatus QuickSyncVideoDecoder::doProcessingStep( QSharedPointer<SurfaceContext> decodedFrameCtx, QSharedPointer<SurfaceContext>* const scaledFrameCtx )
{
    for( ;; )
    {
        *scaledFrameCtx = findUnlockedSurface( &m_scaledFramePool );
        if( !scaledFrameCtx->data() )
        {
            NX_LOG( QString::fromLatin1("No empty frame for processing. Waiting for a frame to get free..."), cl_logERROR );
            //waiting for a surface to get free
            ::Sleep( MS_TO_WAIT_FOR_DEVICE );
            continue;
        }
        break;
    }
    NX_ASSERT( scaledFrameCtx->data() );
    NX_ASSERT( (*scaledFrameCtx)->mfxSurface.Data.Locked == 0 );

    mfxStatus opStatus = MFX_ERR_NONE;

#if defined(SCALE_WITH_MFX)
    opStatus = m_processor->RunFrameVPPAsync( &decodedFrameCtx->mfxSurface, &(*scaledFrameCtx)->mfxSurface, NULL, &m_syncPoint );
    NX_LOG( QString::fromLatin1("QuickSyncVideoDecoder. starting frame (pts %1) processing. Result %2").
        arg((*scaledFrameCtx)->mfxSurface.Data.TimeStamp).arg(mfxStatusCodeToString(opStatus)), opStatus == MFX_ERR_NONE ? cl_logDEBUG2 : cl_logDEBUG1 );
#else
    // Scaling with directx.
    opStatus = scaleFrame( decodedFrameCtx->mfxSurface, &(*scaledFrameCtx)->mfxSurface );
#endif

    return opStatus;
}
