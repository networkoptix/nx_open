/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#include <intrin.h>

#include <Winsock2.h>

#include <algorithm>

#ifndef XVBA_TEST
#include <libavcodec/avcodec.h>
#endif
#include <QDebug>
#include <QElapsedTimer>

#ifndef XVBA_TEST
#include <utils/media/nalunits.h>
#else
#include "nalunits.h"
#endif

#include "quicksyncvideodecoder.h"

//#define RETURN_D3D_SURFACE
#define CONVERT_NV12_TO_YV12


//TODO/IMPL make decoder asynchronous. Fix visual artifacts
//TODO/IMPL take sequence header from extradata. Test on file
//TODO/IMPL support last frame
//TODO/IMPL implement shader NV12 -> RGB


//!Implementation of QnAbstractPictureData
QnAbstractPictureData::PicStorageType D3DPictureData::type() const
{
    return QnAbstractPictureData::pstD3DSurface;
}


QuickSyncVideoDecoder::QuicksyncDXVAPictureData::QuicksyncDXVAPictureData(
    AbstractMFXFrameAllocator* const allocator,
    const QSharedPointer<SurfaceContext>& surface,
    const QRect& _cropRect )
:
    m_mfxSurface( surface ),
    m_d3dSurface( NULL ),
    m_cropRect( _cropRect )
{
    _InterlockedIncrement16( (short*)&m_mfxSurface->mfxSurface.Data.Locked );
    allocator->gethdl( m_mfxSurface->mfxSurface.Data.MemId, reinterpret_cast<mfxHDL*>(&m_d3dSurface) );
    m_d3dSurface->AddRef();
}

QuickSyncVideoDecoder::QuicksyncDXVAPictureData::~QuicksyncDXVAPictureData()
{
    m_d3dSurface->Release();
    _InterlockedDecrement16( (short*)&m_mfxSurface->mfxSurface.Data.Locked );
}

IDirect3DSurface9* QuickSyncVideoDecoder::QuicksyncDXVAPictureData::getSurface() const
{
    return m_d3dSurface;
}

QRect QuickSyncVideoDecoder::QuicksyncDXVAPictureData::cropRect() const
{
    return m_cropRect;
}


//////////////////////////////////////////////////////////
// QuickSyncVideoDecoder
//////////////////////////////////////////////////////////

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
    const QnCompressedVideoDataPtr data,
    PluginUsageWatcher* const pluginUsageWatcher )
:
    m_parentSession( parentSession ),
    m_pluginUsageWatcher( pluginUsageWatcher ),
    m_state( notInitialized ),
    m_syncPoint( NULL ),
    m_mfxSessionEstablished( false ),
    m_decodingInitialized( false ),
    m_totalBytesDropped( 0 ),
    m_prevTimestamp( 0 ),
#ifndef DISABLE_LAST_FRAME
    m_lastAVFrame( NULL ),
#endif
#ifdef USE_D3D
    m_frameAllocator( &m_sysMemFrameAllocator, &m_d3dFrameAllocator ),
#endif
    m_outPictureSize( 0, 0 ),
    m_originalFrameCropTop( 0 ),
    m_originalFrameCropBottom( 0 ),
    m_frameCropTopActual( 0 ),
    m_frameCropBottomActual( 0 ),
    m_reinitializeNeeded( false )
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

#ifndef DISABLE_LAST_FRAME
    m_lastAVFrame = avcodec_alloc_frame();
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

    //TODO/IMPL extradata
    //init( MFX_CODEC_AVC, &inputStream );
    decode( &inputStream, NULL );
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
    closeMFXSession();

#ifndef DISABLE_LAST_FRAME
    av_free( m_lastAVFrame );
#endif
}

PixelFormat QuickSyncVideoDecoder::GetPixelFormat() const
{
#ifndef CONVERT_NV12_TO_YV12
    return PIX_FMT_NV12;
#else
    return PIX_FMT_YUV420P;
#endif
}

#ifndef XVBA_TEST
static const AVRational SRC_DATA_TIMESTAMP_RESOLUTION = {1, 1000000};
static const AVRational INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION = {1, 90000};
#endif

bool QuickSyncVideoDecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
    Q_ASSERT( data->compressionType == CODEC_ID_H264 );

#ifdef WRITE_INPUT_STREAM_TO_FILE_1
    m_inputStreamFile.write( data->data.data(), data->data.size() );
#endif

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    //inputStream->TimeStamp = av_rescale_q( data->timestamp, SRC_DATA_TIMESTAMP_RESOLUTION, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION );
    inputStream.TimeStamp = data->timestamp;

#ifndef XVBA_TEST
    if( (m_state < decoding) && data->context && data->context->ctx() && data->context->ctx()->extradata_size >= 7 && data->context->ctx()->extradata[0] == 1 )
    {
        std::basic_string<mfxU8> seqHeader;
        //sps & pps is in the extradata, parsing it...
        // prefix is unit len
        int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;

        const mfxU8* curNal = data->context->ctx()->extradata+5;
        const mfxU8* dataEnd = data->context->ctx()->extradata + data->context->ctx()->extradata_size;
        while( curNal < dataEnd - reqUnitSize )
        {
            unsigned int curSize = 0;
            for( int i = 0; i < reqUnitSize; ++i ) 
                curSize = (curSize << 8) + curNal[i];
            curNal += reqUnitSize;
            curSize = std::min<>(curSize, (unsigned int)(dataEnd - curNal));
            seqHeader.append( curNal, curSize );

            curNal += curSize;
        }

        seqHeader.append( (const quint8*)data->data.data(), data->data.size() );

        inputStream.Data = const_cast<mfxU8*>(seqHeader.data());
        inputStream.DataLength = seqHeader.size();
        inputStream.MaxLength = seqHeader.size();
        return decode( &inputStream, outFrame );
    }
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

    return decode( &inputStream, outFrame );
}

#ifdef USE_ASYNC_IMPL
bool QuickSyncVideoDecoder::decode( mfxBitstream* const inputStream, CLVideoDecoderOutput* outFrame )
{
    cl_log.log( QString("QuickSyncVideoDecoder::decode (async). data.size = %1").arg(inputStream->DataLength), cl_logDEBUG1 );

    if( (m_state < decoding) && !init( MFX_CODEC_AVC, inputStream ) )
        return false;

    AsyncOperationContext* frameToDisplay = NULL;
    checkAsyncOperations( &frameToDisplay );

    if( frameToDisplay )
    {
        //providing picture to output
        frameToDisplay->state = iisOutForDisplay;
        saveToAVFrame( outFrame, frameToDisplay->outPicture );
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
                cl_log.log( QString::fromAscii("Can't find unused surface for decoding! Waiting for some async operation to complete..."), cl_logDEBUG1 );
                //waiting for surface to become free
                ::Sleep( ASYNC_OPERATION_WAIT_MS );
                AsyncOperationContext* dummyFrameToDisplay = NULL;
                checkAsyncOperations( &dummyFrameToDisplay ); //checking, whether some async operation has completed
                continue;
            }
            cl_log.log( QString::fromAscii("Got unused surface for decoding. Proceeding with async operation"), cl_logDEBUG1 );
            break;
        }
        if( inputStream->DataLength == 0 )
            cl_log.log( QString::fromAscii("Draining decoded frames with NULL input"), cl_logDEBUG1 );

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
            cl_log.log( QString::fromAscii("Started new async decode operation (pts %1). There are %2 operations in total").
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
                cl_log.log( QString::fromAscii("Critical error occured during Quicksync decode session %1").arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
                resetMfxSession();
                return false;
        }

        if( inputStream->DataLength > 0 )
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
                return frameToDisplay != NULL;
            }
            prevInputStreamDataOffset = inputStream->DataOffset;
            continue;
        }

        break;
    }

    return frameToDisplay != NULL;
}
#else
bool QuickSyncVideoDecoder::decode( mfxBitstream* const inputStream, CLVideoDecoderOutput* outFrame )
{
    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder::decode. data.size = %1").arg(inputStream->DataLength), cl_logDEBUG1 );

    if( m_reinitializeNeeded && readSequenceHeader( inputStream ) )
    {
        //have to wait for a sequence header and I-frame before reinitializing decoder
        closeMFXSession();
        m_reinitializeNeeded = false;
    }
    if( m_state < decoding && !init( MFX_CODEC_AVC, inputStream ) )
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
                {
                    QSharedPointer<SurfaceContext> workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
                    Q_ASSERT( workingSurface.data() );
                    opStatus = m_decoder->DecodeFrameAsync(
                        (inputStream && inputStream->DataLength > 0) ? inputStream : NULL,
                        &workingSurface->mfxSurface,
                        &decodedFrame,
                        &m_syncPoint );
#if defined(WRITE_INPUT_STREAM_TO_FILE_2)
                    if( inputStream->DataLength > 0 )
                        m_inputStreamFile.write( (const char*)inputStream->Data + inputStream->DataOffset, inputStream->DataLength );
#endif
                    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. starting frame (pts %1) decoding. Result %2").
                        arg(inputStream->TimeStamp).arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
                    break;  
                }

                case processing:
                {
                    QSharedPointer<SurfaceContext> in = decodedFrameCtx;
                    decodedFrameCtx = findUnlockedSurface( &m_vppOutSurfacePool );
                    Q_ASSERT( decodedFrameCtx.data() );
                    opStatus = m_processor->RunFrameVPPAsync( &in->mfxSurface, &decodedFrameCtx->mfxSurface, NULL, &m_syncPoint );
                    m_state = done;
                    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. starting frame (pts %1) processing. Result %2").
                        arg(decodedFrameCtx->mfxSurface.Data.TimeStamp).arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );
                    break;
                }
            }

            if( opStatus >= MFX_ERR_NONE && m_syncPoint )
            {
                opStatus = MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE );
                cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. sync operation result %1").arg(mfxStatusCodeToString(opStatus)), cl_logDEBUG1 );

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
                    m_state = m_processor.get() ? processing : done;
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
                    cl_log.log( QString::fromAscii("Providing picture at output. Out frame timestamp %1, frame order %2, corrupted %3").
                        arg(decodedFrameCtx->mfxSurface.Data.TimeStamp).arg(decodedFrameCtx->mfxSurface.Data.FrameOrder).arg(decodedFrameCtx->mfxSurface.Data.Corrupted), cl_logDEBUG1 );
                    if( m_prevTimestamp > decodedFrameCtx->mfxSurface.Data.TimeStamp )
                    {
                        cl_log.log( QString::fromAscii("Warning! timestamp decreased by %1. Current 90KHz timestamp %2, previous %3. Ignoring frame...").
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
                        saveToAVFrame( outFrame, decodedFrameCtx );
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
                cl_log.log( QString::fromAscii("Critical error occured during Quicksync decode session %1").arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
                resetMfxSession();
                return false;
        }

        if( inputStream->DataLength > 0 )
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
#endif

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
    return m_outPictureSize.isEmpty()
        ? m_srcStreamParam.mfx.FrameInfo.Width
        : m_outPictureSize.width();
}

//!Implementation of QnAbstractVideoDecoder::getHeight
int QuickSyncVideoDecoder::getHeight() const
{
    return m_outPictureSize.isEmpty()
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
void QuickSyncVideoDecoder::resetDecoder( QnCompressedVideoDataPtr data )
{
    if( m_decodingInitialized )
        m_decoder->Reset( &m_srcStreamParam );
}
#endif

#ifndef DISABLE_LAST_FRAME
const AVFrame* QuickSyncVideoDecoder::lastFrame() const
{
    return m_lastAVFrame;
}
#endif

void QuickSyncVideoDecoder::setOutPictureSize( const QSize& outSize )
{
    const QSize alignedNewPicSize = QSize( ALIGN16(outSize.width()), ALIGN32(outSize.height()) );
    if( m_outPictureSize == alignedNewPicSize )
        return;

    m_outPictureSize = alignedNewPicSize;
    cl_log.log( QString::fromAscii("Quicksync decoder. Setting up scaling to %1x%2").arg(m_outPictureSize.width()).arg(m_outPictureSize.height()), cl_logINFO );
    m_reinitializeNeeded = true;

    //if( !m_outPictureSize.isEmpty() )
    //{
    //    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. Output picture size can be set only once. "
    //        "Using picture size %1x%2").arg(m_outPictureSize.width(), m_outPictureSize.height()), cl_logDEBUG1 );
    //    //TODO/IMPL support changing of output picture size
    //    return;
    //}

    //m_outPictureSize = QSize( ALIGN16(outSize.width()), ALIGN32(outSize.height()) );

    ////creating processing
    //m_processor.reset();
    ////if( m_vppAllocResponse.NumFrameActual > 0 )
    ////{
    ////    m_frameAllocator._free( &m_vppAllocResponse );
    ////    memset( &m_vppAllocResponse, 0, sizeof(m_vppAllocResponse) );
    ////}
    //if( m_decodingInitialized )
    //    if( !initProcessor() )
    //        m_outPictureSize = QSize();
}

QnAbstractPictureData::PicStorageType QuickSyncVideoDecoder::targetMemoryType() const
{
    return QnAbstractPictureData::pstSysMemPic;
}

bool QuickSyncVideoDecoder::init( mfxU32 codecID, mfxBitstream* seqHeader )
{
    readSequenceHeader( seqHeader );
    m_frameCropTopActual = m_originalFrameCropTop;  //in case we do not need VPP
    m_frameCropBottomActual = m_originalFrameCropBottom;

    if( !m_mfxSessionEstablished && !initMfxSession() )
        return false;

    if( !m_decodingInitialized && !initDecoder( codecID, seqHeader ) )
        return false;

    if( processingNeeded() && !m_processor.get() && !initProcessor() )
        return false;

    m_state = decoding;

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

    if( m_parentSession )
    {
        status = MFXJoinSession( *m_parentSession, m_mfxSession );
        if( status != MFX_ERR_NONE )
        {
            cl_log.log( QString::fromAscii("Failed to join Intel media SDK session with parent session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
            //m_mfxSession.Close();
            //return false;
        }
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

    mfxStatus status = MFX_ERR_NONE;
    m_srcStreamParam.mfx.CodecId = codecID;
    status = m_decoder->DecodeHeader( seqHeader, &m_srcStreamParam );
    switch( status )
    {
        case MFX_ERR_NONE:
            break;

        case MFX_ERR_MORE_DATA:
            cl_log.log( QString::fromAscii("Need more data to parse sequence header"), cl_logWARNING );
            if( m_cachedSequenceHeader.empty() )
                m_cachedSequenceHeader.append( seqHeader->Data, seqHeader->MaxLength );
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
    m_cachedSequenceHeader.clear();

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
#ifdef USE_ASYNC_IMPL
    m_srcStreamParam.AsyncDepth = 0;    //TODO
#else
    m_srcStreamParam.AsyncDepth = 1;
#endif
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

    if( m_outPictureSize == QSize(m_srcStreamParam.mfx.FrameInfo.Width, m_srcStreamParam.mfx.FrameInfo.Height - (m_originalFrameCropTop + m_originalFrameCropBottom)) )
    {
        //no need for scaling
        m_frameCropTopActual = m_originalFrameCropTop;
        m_frameCropBottomActual = m_originalFrameCropBottom;
        m_outPictureSize.setWidth( 0 );
        m_outPictureSize.setHeight( 0 );
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
#ifdef USE_D3D
    m_processingParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#else
    m_processingParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#endif
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
        cl_log.log( QString::fromAscii("Failed to query VPP. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }
    else if( status > MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Warning from quering VPP. Status %1").arg(mfxStatusCodeToString(status)), cl_logWARNING );
    }

    status = m_processor->QueryIOSurf( &m_processingParams, request );
    if( status < MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to query surface information for VPP from Intel media SDK session. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }
    else if( status > MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Warning from quering VPP (io surf). Status %1").arg(mfxStatusCodeToString(status)), cl_logWARNING );
    }

    //U00, V00, U01, V01, U10, V10, U11, V11;
    //U00, U01, U10, U11, V00, V01, V10, V11;
    //U00, U10, V00, V10, U01, U11, V01, V11;
    //U00, V00, U01, V01, U10, V10, U11, V11;

    //as input surfaces will use decoder output

    request[1].NumFrameSuggested = std::max<size_t>( request[1].NumFrameSuggested, m_decoderSurfacePool.size() );

    allocateSurfacePool( &m_vppOutSurfacePool, &request[1], &m_vppAllocResponse );
    for( int i = 0; i < m_vppOutSurfacePool.size(); ++i )
        m_vppOutSurfacePool[i]->mfxSurface.Info = m_processingParams.vpp.Out;

    status = m_processor->Init( &m_processingParams );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to initialize Intel media SDK processor. Status %1").arg(mfxStatusCodeToString(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    cl_log.log( QString::fromAscii("Intel Media processor (scale to %1x%2) successfully initialized!. Allocated %3 surfaces in %4 memory").
        arg(m_outPictureSize.width()).arg(m_outPictureSize.height()).
        arg(m_vppOutSurfacePool.size()).
#ifdef USE_D3D
        arg(QString::fromAscii("video")),
#else
        arg(QString::fromAscii("system")),
#endif
        cl_logINFO );

    return true;
}

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
    m_processor.reset();

    m_decoder.reset();
    m_decodingInitialized = false;

    m_mfxSession.Close();
    m_mfxSessionEstablished = false;

    //free surface pool
    if( m_decodingAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_decodingAllocResponse );
    m_decoderSurfacePool.clear();
    if( m_vppAllocResponse.NumFrameActual > 0 )
        m_frameAllocator._free( &m_vppAllocResponse );
    m_vppOutSurfacePool.clear();

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
                it->outPicture = findUnlockedSurface( &m_vppOutSurfacePool );
                if( !it->outPicture.data() )
                {
                    //NOTE it is normal, that we cannot find unused surface for vpp, since we give out vpp_out surfaces to the caller and it is unspecified, 
                        //when it will release vpp_out surface. Because of this we need all this states & transitions
                    cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. Cannot start VPP of frame (pts %1), since there is no unused vpp_out surface").
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

                cl_log.log( QString::fromAscii("QuickSyncVideoDecoder. starting frame (pts %1) post-processing. Result %2").
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

                ////marking it->outPicture surface as unused
                //markSurfaceAsUnused( it->outPicture );
                ////locking surface
                //_InterlockedIncrement16( (short*)&it->outPicture->Data.Locked );
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
            //        cl_log.log( QString::fromAscii("Warning! Intel media decoder left %1 bytes of data (status %2). Dropping these bytes... Total bytes dropped %3").
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
                    cl_log.log( QString::fromAscii("Critical error occured during Quicksync decode session %1").arg(mfxStatusCodeToString(opStatus)), cl_logERROR );
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
    for( int i = 0; i < surfacePool->size(); ++i )
    {
        if( surfacePool->at(i)->mfxSurface.Data.Locked == 0 )
            return surfacePool->at(i);
    }
    return QSharedPointer<SurfaceContext>();
}

bool QuickSyncVideoDecoder::processingNeeded() const
{
    return !m_outPictureSize.isNull();
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

//inline void deinterleave( __m128i i1, __m128i i2, __m128i* o1, __m128i* o2 )
//{
//}

void QuickSyncVideoDecoder::deinterleaveUVPlane(
    __m128i* nv12UVPlane,
    size_t nv12UVPlaneSize,
    __m128i* yv12UPlane,
    __m128i* yv12VPlane )
{
    //static const __m128i MASK_LO = { 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff };
    static const __m128i MASK_HI = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };
    static const __m128i AND_MASK = MASK_HI;
    static const __m128i SHIFT_8 = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const __m128i* const src_end = nv12UVPlane + nv12UVPlaneSize / sizeof(__m128i);
    while( nv12UVPlane < src_end )
    {
        __m128i x1 = _mm_stream_load_si128( nv12UVPlane );
        __m128i x2 = _mm_stream_load_si128( nv12UVPlane+1 );
        __m128i x3 = _mm_stream_load_si128( nv12UVPlane+2 );
        __m128i x4 = _mm_stream_load_si128( nv12UVPlane+3 );

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

        nv12UVPlane += 4;
    }

    //__m128i x5, x6;
    //x5 = _mm_unpacklo_epi8( x1, x2 );
    //x6 = _mm_unpackhi_epi8( x1, x2 );
    //x1 = _mm_unpacklo_epi16( x5, x6 );
    //x2 = _mm_unpackhi_epi16( x5, x6 );
    //x5 = _mm_unpacklo_epi32( x1, x2 );
    //x6 = _mm_unpackhi_epi32( x1, x2 );
    //x1 = _mm_unpacklo_epi64( x5, x6 );
    //x2 = _mm_unpackhi_epi64( x5, x6 );
    //_mm_store_si128( yv12UPlane, x1 );
    //_mm_store_si128( yv12VPlane, x2 );
}

void QuickSyncVideoDecoder::saveToAVFrame( CLVideoDecoderOutput* outFrame, const QSharedPointer<SurfaceContext>& decodedFrameCtx )
{
    const mfxFrameSurface1* const decodedFrame = &decodedFrameCtx->mfxSurface;

#ifdef RETURN_D3D_SURFACE
    outFrame->picData = QSharedPointer<QnAbstractPictureData>(
        new QuicksyncDXVAPictureData(
            &m_frameAllocator,
            decodedFrameCtx,
            QRect(0, m_frameCropTopActual, decodedFrame->Info.Width, decodedFrame->Info.Height - (m_frameCropTopActual + m_frameCropBottomActual)) ) );
#else
    //outFrame = decodedFrame
    m_frameAllocator.lock( decodedFrameCtx->mfxSurface.Data.MemId, &decodedFrameCtx->mfxSurface.Data );

    //if( !outFrame->isExternalData() )
    {
        //copying frame data if needed
        outFrame->reallocate(
            decodedFrame->Data.Pitch,
            decodedFrame->Info.Height - (m_frameCropTopActual + m_frameCropBottomActual),
#ifndef CONVERT_NV12_TO_YV12
            PIX_FMT_NV12,
#else
            PIX_FMT_YUV420P,
#endif
            decodedFrame->Data.Pitch );

        fastmemcpy_sse4(
            (__m128i*)outFrame->data[0],
            (__m128i*)(decodedFrame->Data.Y + (m_frameCropTopActual * decodedFrame->Data.Pitch)),
            decodedFrame->Data.Pitch * (decodedFrame->Info.Height - m_frameCropBottomActual) );
        //QElapsedTimer t1;
        //t1.start();
        //for( int i = 0; i < 10000; ++i )
        //{
#ifndef CONVERT_NV12_TO_YV12
        fastmemcpy_sse4(
            (__m128i*)outFrame->data[1],
            (__m128i*)(decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch)),
            decodedFrame->Data.Pitch * (decodedFrame->Info.Height - m_frameCropBottomActual) / 2 );
#else
        deinterleaveUVPlane(
            (__m128i*)(decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch)),
            decodedFrame->Data.Pitch * (decodedFrame->Info.Height - m_frameCropBottomActual) / 2,
            (__m128i*)outFrame->data[1],
            (__m128i*)outFrame->data[2] );
#endif
        //}
        //qDebug() << "!!!!!!!!!!!!!"<<t1.elapsed()<<"!!!!!!!!!!!!!1";
    }
    //else
    //{
    //    outFrame->data[0] = decodedFrame->Data.Y + m_frameCropTopActual * decodedFrame->Data.Pitch;
    //    outFrame->data[1] = decodedFrame->Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
    //    outFrame->data[2] = decodedFrame->Data.V + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
    //}
    outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
#ifndef CONVERT_NV12_TO_YV12
    outFrame->linesize[1] = decodedFrame->Data.Pitch;       //uv_stride
    outFrame->linesize[2] = 0;
#else
    outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //u_stride
    outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;   //v_stride
#endif
#endif

    outFrame->width = decodedFrame->Info.Width;
    outFrame->height = decodedFrame->Info.Height - (m_frameCropTopActual + m_frameCropBottomActual);
#ifndef CONVERT_NV12_TO_YV12
    outFrame->format = PIX_FMT_NV12;
#else
    outFrame->format = PIX_FMT_YUV420P;
#endif
    //outFrame->key_frame = decodedFrame->Data.
    //outFrame->pts = av_rescale_q( decodedFrame->Data.TimeStamp, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION, SRC_DATA_TIMESTAMP_RESOLUTION );
    outFrame->pts = decodedFrame->Data.TimeStamp;
    outFrame->pkt_dts = decodedFrame->Data.TimeStamp;

    outFrame->display_picture_number = decodedFrame->Data.FrameOrder;
    outFrame->interlaced_frame = 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_REPEATED;

#ifndef RETURN_D3D_SURFACE

#ifndef DISABLE_LAST_FRAME
    *m_lastAVFrame = *outFrame;
    m_lastAVFrame->data[0] = decodedFrameCtx->mfxSurface.Data.Y + m_frameCropTopActual * decodedFrame->Data.Pitch;
    m_lastAVFrame->data[1] = decodedFrameCtx->mfxSurface.Data.U + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
    m_lastAVFrame->data[2] = decodedFrameCtx->mfxSurface.Data.V + (m_frameCropTopActual/2 * decodedFrame->Data.Pitch);
#endif

    m_frameAllocator.unlock( decodedFrameCtx->mfxSurface.Data.MemId, &decodedFrameCtx->mfxSurface.Data );
#endif
}

bool QuickSyncVideoDecoder::readSequenceHeader( mfxBitstream* const inputStream )
{
    SPSUnit m_sps;
    PPSUnit m_pps;

    if( inputStream->DataLength <= 4 )
        return false;

    //memset( &m_pictureDescriptor, 0, sizeof(m_pictureDescriptor) );

    bool spsFound = false;
    bool ppsFound = false;
    const quint8* dataStart = reinterpret_cast<const quint8*>(inputStream->Data + inputStream->DataOffset);
    const quint8* dataEnd = dataStart + inputStream->DataLength;
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
                //qDebug()<<"Parsing sps\n";
                m_sps.decodeBuffer( curNalu, nextNalu );
                m_sps.deserialize();
                spsFound = true;

                //reading frame cropping settings
                const unsigned int SubHeightC = m_sps.chroma_format_idc == 1 ? 2 : 1;
                const unsigned int CropUnitY = (m_sps.chroma_format_idc == 0)
                    ? (2 - m_sps.frame_mbs_only_flag)
                    : (SubHeightC * (2 - m_sps.frame_mbs_only_flag));
                m_originalFrameCropTop = CropUnitY * m_sps.frame_crop_top_offset;
                m_originalFrameCropBottom = CropUnitY * m_sps.frame_crop_bottom_offset;

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
        i < m_vppOutSurfacePool.size();
        ++i )
    {
        if( &m_vppOutSurfacePool[i]->mfxSurface == surf )
            return m_vppOutSurfacePool[i];
    }

    return QSharedPointer<SurfaceContext>();
}

void QuickSyncVideoDecoder::resetMfxSession()
{
    cl_log.log( QString::fromAscii("Resetting Quicksync decode session..."), cl_logERROR );

    m_decoder->Reset( &m_srcStreamParam );
    if( m_processor.get() )
        m_processor->Reset( &m_processingParams );

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
