/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#include "quicksyncvideodecoder.h"

#ifndef _DEBUG

//#include <Windows.h>

#include <iostream>

#include "../../utils/media/nalunits.h"

#ifdef _WIN32
#endif


//////////////////////////////////////////////////////////
// Allocator (example from Intel Media SDK )
//////////////////////////////////////////////////////////

typedef struct {
    //!these values are always aligned to 32-byte boundary top
    mfxU16 width, height;
    mfxU8* base;
    mfxU16 chromaFormat;
    mfxU32 fourCC;
} mid_struct;


template<class Numeric> Numeric ALIGN16( Numeric n )
{
    Numeric tmp = n & 0x0F;
    return tmp == 0 ? n : (n - tmp + 16);
}

template<class Numeric> Numeric ALIGN32( Numeric n )
{
    Numeric tmp = n & 0x1F;
    return tmp == 0 ? n : (n - tmp + 32);
}


MyBufferAllocator::MyBufferAllocator()
{
    memset( reserved, 0, sizeof(reserved) );
    pthis = this;
    Alloc = ba_alloc;
    Lock = ba_lock;
    Unlock = ba_unlock;
    Free = ba_free;
}

mfxStatus MyBufferAllocator::ba_alloc( mfxHDL /*pthis*/, mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid )
{
    *mid = malloc(nbytes);
    return (*mid) ? MFX_ERR_NONE : MFX_ERR_MEMORY_ALLOC;
}

mfxStatus MyBufferAllocator::ba_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxU8 **ptr )
{
    *ptr = (mfxU8*)mid;
    return MFX_ERR_NONE;
}

mfxStatus MyBufferAllocator::ba_unlock(mfxHDL /*pthis*/, mfxMemId /*mid*/)
{
    return MFX_ERR_NONE;
}

mfxStatus MyBufferAllocator::ba_free( mfxHDL /*pthis*/, mfxMemId mid ) 
{
    if (mid)
        free((mfxU8*)mid);
    return MFX_ERR_NONE;
}


MyFrameAllocator::MyFrameAllocator()
{
    memset( reserved, 0, sizeof(reserved) );
    pthis = this;
    Alloc = fa_alloc;
    Lock = fa_lock;
    Unlock = fa_unlock;
    GetHDL = fa_gethdl;
    Free = fa_free;
}

mfxStatus MyFrameAllocator::fa_alloc( mfxHDL /*pthis*/, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response )
{
    if( !(request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) )
        return MFX_ERR_UNSUPPORTED;
    if( (request->Info.FourCC != MFX_FOURCC_NV12) && (request->Info.FourCC != MFX_FOURCC_YV12) )
        return MFX_ERR_UNSUPPORTED;
    response->NumFrameActual = request->NumFrameMin;
    response->mids = (mfxMemId*)malloc( response->NumFrameActual * sizeof(mid_struct*) );
    for( int i=0; i<request->NumFrameMin; ++i )
    {
        mid_struct* mmid = (mid_struct *)malloc(sizeof(mid_struct));
        mmid->width = ALIGN16( request->Info.Width );
        mmid->height = ALIGN32( request->Info.Height );
        mmid->chromaFormat = request->Info.ChromaFormat;
        mmid->fourCC = request->Info.FourCC;
        if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420 )
            mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*3/2);
        else if( request->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV422 )
            mmid->base = (mfxU8*)malloc(mmid->width*mmid->height*2);
        response->mids[i] = mmid;
    }
    return MFX_ERR_NONE;
}

mfxStatus MyFrameAllocator::fa_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxFrameData* ptr )
{
    //works only for YUV
    mid_struct* mmid = (mid_struct *)mid;
    ptr->Pitch = mmid->width;
    ptr->Y = mmid->base;
    if( mmid->chromaFormat == MFX_CHROMAFORMAT_YUV420 )
    {
        ptr->U = ptr->Y + mmid->width * mmid->height;
        if( mmid->fourCC == MFX_FOURCC_NV12 )
            ptr->V = ptr->U + 1;
        else if( mmid->fourCC == MFX_FOURCC_YV12 )
            ptr->V = ptr->U + (mmid->width * mmid->height) / 4;
    }
    else if( mmid->chromaFormat == MFX_CHROMAFORMAT_YUV422 )
    {
        ptr->U = ptr->Y + mmid->width * mmid->height;
        ptr->V = ptr->U + (mmid->width * mmid->height) / 2;
    }
    return MFX_ERR_NONE;
}

mfxStatus MyFrameAllocator::fa_unlock( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxFrameData* ptr )
{
    if( ptr )
        ptr->Y = ptr->U = ptr->V = ptr->A = NULL;
    return MFX_ERR_NONE;
}

mfxStatus MyFrameAllocator::fa_gethdl( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxHDL* /*handle*/ )
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MyFrameAllocator::fa_free( mfxHDL /*pthis*/, mfxFrameAllocResponse* response )
{
    for( int i = 0; i<response->NumFrameActual; i++ )
    {
        mid_struct* mmid = (mid_struct*)response->mids[i];
        free( mmid->base );
        free( mmid );
    }
    free( response->mids );
    return MFX_ERR_NONE;
}



//////////////////////////////////////////////////////////
// QuickSyncVideoDecoder
//////////////////////////////////////////////////////////

//!Maximum tries to decode frame
static const int MAX_ENCODE_ASYNC_CALL_TRIES = 7;
static const int MS_TO_WAIT_FOR_DEVICE = 5;

static const PixelFormat pixelFormat = PIX_FMT_NV12;
//static const PixelFormat pixelFormat = PIX_FMT_YUV420P;

QuickSyncVideoDecoder::QuickSyncVideoDecoder()
:
    m_state( decoding ),
    m_syncPoint( NULL ),
    m_initialized( false ),
    m_mfxSessionEstablished( false ),
    m_decodingInitialized( false ),
    m_totalBytesDropped( 0 ),
    m_prevTimestamp( 0 ),
    m_lastAVFrame( NULL ),
    m_outPictureSize( 0, 0 )
{
    memset( &m_srcStreamParam, 0, sizeof(m_srcStreamParam) );
    memset( &m_decodingAllocResponse, 0, sizeof(m_decodingAllocResponse) );
    memset( &m_vppAllocResponse, 0, sizeof(m_vppAllocResponse) );

    m_lastAVFrame = avcodec_alloc_frame();

    //setOutPictureSize( QSize( 320, 240 ) );

#ifdef WRITE_INPUT_STREAM_TO_FILE
    m_inputStreamFile.open( "C:/Content/earth.264", std::ios::binary );
#endif
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
    m_processor.reset();
    m_decoder.reset();
    m_mfxSession.Close();

    //free surface pool
    if( m_decodingAllocResponse.NumFrameActual > 0 )
        m_frameAllocator.Free( &m_frameAllocator, &m_decodingAllocResponse );
    if( m_vppAllocResponse.NumFrameActual > 0 )
        m_frameAllocator.Free( &m_frameAllocator, &m_vppAllocResponse );

    av_free( m_lastAVFrame );
}

PixelFormat QuickSyncVideoDecoder::GetPixelFormat() const
{
    return pixelFormat;
}

static const AVRational SRC_DATA_TMESTAMP_RESOLUTION = {1, 1000000};
static const AVRational INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION = {1, 90000};

bool QuickSyncVideoDecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
    if( !m_initialized )
        readSequenceHeader( data );

    mfxBitstream inputStream;
    memset( &inputStream, 0, sizeof(inputStream) );
    //inputStream.TimeStamp = av_rescale_q( data->timestamp, SRC_DATA_TMESTAMP_RESOLUTION, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION );
    inputStream.TimeStamp = data->timestamp;
    //inputStream.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    //if( data->context && data->context->ctx() && data->context->ctx()->extradata_size >= 7 && data->context->ctx()->extradata[0] == 1 )
    //{
    //    std::basic_string<mfxU8> seqHeader;
    //    //TODO/IMPL sps & pps is in the extradata, parsing it...
    //    // prefix is unit len
    //    int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;

    //    const mfxU8* curNal = data->context->ctx()->extradata+5;
    //    const mfxU8* dataEnd =  data->context->ctx()->extradata +  data->context->ctx()->extradata_size;
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
#endif

    //qDebug()<<"Input timestamp: "<<inputStream.TimeStamp;
    //if( m_prevTimestamp > inputStream.TimeStamp )
    //    qDebug()<<"Warning! timestamp decreased by "<<(m_prevTimestamp - inputStream.TimeStamp);
    //m_prevTimestamp = inputStream.TimeStamp;

    inputStream.Data = reinterpret_cast<mfxU8*>(data->data.data());
    inputStream.DataLength = data->data.size();
    inputStream.MaxLength = data->data.size();
    //inputStream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    return decode( data->compressionType == CODEC_ID_H264 ? MFX_CODEC_AVC : MFX_CODEC_MPEG2, &inputStream, outFrame );
}

bool QuickSyncVideoDecoder::decode( mfxU32 codecID, mfxBitstream* inputStream, CLVideoDecoderOutput* outFrame )
{
    if( !m_initialized && !init( codecID, inputStream ) )
        return false;

    bool gotDisplayPicture = false;

    mfxFrameSurface1* decodedFrame = NULL;
    unsigned int prevInputStreamDataOffset = inputStream->DataOffset;
    for( int decodingTry = 0;
        decodingTry < MAX_ENCODE_ASYNC_CALL_TRIES;
         )
    {
        //mfxFrameSurface1* workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
        //if( workingSurface )
        //{
        //    //workingSurface->Info.ChromaFormat = m_srcStreamParam.mfx.FrameInfo.ChromaFormat;
        //    opStatus = m_decoder->DecodeFrameAsync( inputStream, workingSurface, &decodedFrame, &m_syncPoint );
        //}
        //else
        //{
        //    opStatus = m_decoder->DecodeFrameAsync( NULL, NULL, &decodedFrame, &m_syncPoint );
        //}

        //qDebug()<<"Op start status "<<opStatus;

        //if( opStatus >= MFX_ERR_NONE && m_syncPoint )
        //{
        //    opStatus = MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE );
        //}
        //else if( opStatus > MFX_ERR_NONE )  //DecodeFrameAsync returned warning
        //{
        //    if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
        //    {
        //        //читаем новые параметры видео-потока
        //        m_decoder->GetVideoParam( &m_srcStreamParam );
        //    }
        //    continue;
        //}

        //qDebug()<<"Op sync status  "<<opStatus;

        //if( opStatus > MFX_ERR_NONE )
        //    opStatus = MFX_ERR_NONE;   //ignoring warnings

        //if( m_processor.get() && opStatus >= MFX_ERR_NONE )
        //{
        //    mfxFrameSurface1* in = decodedFrame;
        //    decodedFrame = findUnlockedSurface( &m_processingSurfacePool );
        //    opStatus = m_processor->RunFrameVPPAsync( in, decodedFrame, NULL, &m_syncPoint );

        //    if( opStatus >= MFX_ERR_NONE && m_syncPoint )
        //        opStatus = MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE );

        //    if( opStatus > MFX_ERR_NONE )
        //        opStatus = MFX_ERR_NONE;   //ignoring warnings
        //}

        mfxStatus opStatus = MFX_ERR_NONE;
        m_state = decoding;
        while( (m_state != done) && (opStatus >= MFX_ERR_NONE) )
        {
            switch( m_state )
            {
                case decoding:
                {
                    mfxFrameSurface1* workingSurface = findUnlockedSurface( &m_decoderSurfacePool );
                    opStatus = m_decoder->DecodeFrameAsync( workingSurface ? inputStream : NULL, workingSurface, &decodedFrame, &m_syncPoint );
                    break;
                }

                case processing:
                {
                    mfxFrameSurface1* in = decodedFrame;
                    decodedFrame = findUnlockedSurface( &m_processingSurfacePool );
                    opStatus = m_processor->RunFrameVPPAsync( in, decodedFrame, NULL, &m_syncPoint );
                    m_state = done;
                    break;
                }
            }

            qDebug()<<"Op start status "<<opStatus;

            if( opStatus >= MFX_ERR_NONE && m_syncPoint )
            {
                opStatus = MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE );
                qDebug()<<"Op sync status  "<<opStatus;
            }
            else if( opStatus > MFX_ERR_NONE )  //DecodeFrameAsync returned warning
            {
                if( opStatus == MFX_WRN_VIDEO_PARAM_CHANGED )
                {
                    //читаем новые параметры видео-потока
                    m_decoder->GetVideoParam( &m_srcStreamParam );
                }
                opStatus = MFX_ERR_NONE;
                continue;
            }

            if( opStatus > MFX_ERR_NONE )
                opStatus = MFX_ERR_NONE;   //ignoring warnings

            //moving stte
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
                    qDebug()<<"Providing picture at output";
                    //qDebug()<<"Out frame timestamp "<<decodedFrame->Data.TimeStamp<<", frame order "<<decodedFrame->Data.FrameOrder<<", corrupted "<<decodedFrame->Data.Corrupted;
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
                        *m_lastAVFrame = *outFrame;
                        m_lastAVFrame->data[0] = decodedFrame->Data.Y;
                        m_lastAVFrame->data[1] = decodedFrame->Data.U;
                        m_lastAVFrame->data[2] = decodedFrame->Data.V;
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
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
                //trying to reset decoder
                m_decoder->Reset( &m_srcStreamParam );
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

void QuickSyncVideoDecoder::setOutPictureSize( const QSize& outSize )
{
    m_outPictureSize = QSize( ALIGN16(outSize.width()), ALIGN32(outSize.height()) );

    //creating processing
    m_processor.reset();
    if( m_decodingInitialized )
        initProcessor();
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
        cl_log.log( QString::fromAscii("Failed to create Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
    status = m_mfxSession.SetBufferAllocator( &m_bufAllocator );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to set buffer allocator to Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
    status = m_mfxSession.SetFrameAllocator( &m_frameAllocator );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to set frame allocator to Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
        m_mfxSession.Close();
        return false;
    }
    m_mfxSessionEstablished = true;

    //checking, whether hardware acceleration enabled
    mfxIMPL actualImplUsed = MFX_IMPL_HARDWARE;
    m_mfxSession.QueryIMPL( &actualImplUsed );
    m_hardwareAccelerationEnabled = actualImplUsed != MFX_IMPL_SOFTWARE;

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

    m_srcStreamParam.mfx.CodecId = codecID;
    mfxStatus status = m_decoder->DecodeHeader( seqHeader, &m_srcStreamParam );
    switch( status )
    {
        case MFX_ERR_NONE:
            break;
        case MFX_ERR_MORE_DATA:
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
            cl_log.log( QString::fromAscii("Failed to decode stream header during Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
            m_decoder.reset();
            return false;
    }

    cl_log.log( QString::fromAscii("Successfully decoded stream header with Intel Media decoder! Picture resolution %1x%2, stream profile/level: %3/%4").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)),
        cl_logINFO );

    mfxFrameAllocRequest decodingAllocRequest;
    memset( &decodingAllocRequest, 0, sizeof(decodingAllocRequest) );
    m_srcStreamParam.AsyncDepth = std::max<>( m_srcStreamParam.AsyncDepth, (mfxU16)1 );
    //m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
    m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_srcStreamParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    status = m_decoder->Query( &m_srcStreamParam, &m_srcStreamParam );

    decodingAllocRequest.Info = m_srcStreamParam.mfx.FrameInfo;
    status = m_decoder->QueryIOSurf( &m_srcStreamParam, &decodingAllocRequest );
    if( status < MFX_ERR_NONE ) //ignoring warnings
    {
        cl_log.log( QString::fromAscii("Failed to query surface information for decoding from Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
        m_decoder.reset();
        return false;
    }

    //allocating pool of frame surfaces (decodingAllocRequest.NumFrameSuggested)
    allocateSurfacePool( &m_decoderSurfacePool, &decodingAllocRequest, &m_decodingAllocResponse );

    status = m_decoder->Init( &m_srcStreamParam );
    if( status < MFX_ERR_NONE )  //ignoring warnings
    {
        cl_log.log( QString::fromAscii("Failed to initialize Intel media SDK decoder. Status %1").arg(QString::number(status)), cl_logERROR );
        m_decoder.reset();
        return false;
    }

    cl_log.log( QString::fromAscii("Intel Media decoder successfully initialized! Picture resolution %1x%2, stream profile/level: %3/%4. Allocated %5 surfaces in system memory...").
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Width)).
        arg(QString::number(m_srcStreamParam.mfx.FrameInfo.Height)).
        arg(QString::number(m_srcStreamParam.mfx.CodecProfile)).
        arg(QString::number(m_srcStreamParam.mfx.CodecLevel)).
        arg(QString::number(m_decodingAllocResponse.NumFrameActual)),
        cl_logINFO );

    m_decodingInitialized = true;
    return true;
}

bool QuickSyncVideoDecoder::initProcessor()
{
    Q_ASSERT( m_decodingInitialized );

    m_processor.reset( new MFXVideoVPP( m_mfxSession ) );

    mfxVideoParam procesingParams;
    memset( &procesingParams, 0, sizeof(procesingParams) );
    procesingParams.AsyncDepth = 1;
    procesingParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    procesingParams.vpp.In = m_srcStreamParam.mfx.FrameInfo;
    procesingParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    //procesingParams.vpp.In.CropX = 0;
    //procesingParams.vpp.In.CropY = 0;
    //procesingParams.vpp.In.CropW = procesingParams.vpp.In.Width;
    //procesingParams.vpp.In.CropH = procesingParams.vpp.In.Height;
    procesingParams.vpp.In.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    procesingParams.vpp.In.FrameRateExtD = 30;  //must be not null
    procesingParams.vpp.In.FrameRateExtN = 1;
    procesingParams.vpp.Out = m_srcStreamParam.mfx.FrameInfo;
    procesingParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    procesingParams.vpp.Out.Width = m_outPictureSize.width();
    procesingParams.vpp.Out.Height = m_outPictureSize.height();
    procesingParams.vpp.Out.CropX = 0;
    procesingParams.vpp.Out.CropY = 0;
    procesingParams.vpp.Out.CropW = procesingParams.vpp.Out.Width;
    procesingParams.vpp.Out.CropH = procesingParams.vpp.Out.Height;
    procesingParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    procesingParams.vpp.Out.FrameRateExtD = procesingParams.vpp.In.FrameRateExtD;
    procesingParams.vpp.Out.FrameRateExtN = procesingParams.vpp.In.FrameRateExtN;

    mfxFrameAllocRequest request[2];
    memset( request, 0, sizeof(request) );

    mfxStatus status = m_processor->QueryIOSurf( &procesingParams, request );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to query surface information for VPP from Intel media SDK session. Status %1").arg(QString::number(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    //U00, V00, U01, V01, U10, V10, U11, V11;
    //U00, U01, U10, U11, V00, V01, V10, V11;
    //U00, U10, V00, V10, U01, U11, V01, V11;
    //U00, V00, U01, V01, U10, V10, U11, V11;

    allocateSurfacePool( &m_processingSurfacePool, &request[1], &m_vppAllocResponse );
    for( int i = 0; i < m_processingSurfacePool.size(); ++i )
    {
        m_processingSurfacePool[i].Info = procesingParams.vpp.Out;
    }

    status = m_processor->Init( &procesingParams );
    if( status != MFX_ERR_NONE )
    {
        cl_log.log( QString::fromAscii("Failed to initialize Intel media SDK processor. Status %1").arg(QString::number(status)), cl_logERROR );
        m_processor.reset();
        return false;
    }

    cl_log.log( QString::fromAscii("Intel Media processor (from NV12 to YV12) successfully initialized!"), cl_logINFO );

    return true;
}

void QuickSyncVideoDecoder::allocateSurfacePool(
    std::vector<mfxFrameSurface1>* const surfacePool,
    mfxFrameAllocRequest* const allocRequest,
    mfxFrameAllocResponse* const allocResponse )
{
    memset( allocResponse, 0, sizeof(*allocResponse) );
    m_frameAllocator.Alloc( &m_frameAllocator, allocRequest, allocResponse );
    surfacePool->reserve( surfacePool->size() + allocResponse->NumFrameActual );
    for( int i = 0; i < allocResponse->NumFrameActual; ++i )
    {
        surfacePool->push_back( mfxFrameSurface1() );
        memset( &surfacePool->back(), 0, sizeof(mfxFrameSurface1) );
        memcpy( &surfacePool->back().Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
        surfacePool->back().Data.MemId = allocResponse->mids[i];
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
    //TODO/IMPL выделяем память под ещё один кадр
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
        outFrame->reallocate( decodedFrame->Info.Width, decodedFrame->Info.Height, pixelFormat );
        memcpy( outFrame->data[0], decodedFrame->Data.Y, decodedFrame->Data.Pitch * decodedFrame->Info.Height );
        memcpy( outFrame->data[1], decodedFrame->Data.U, decodedFrame->Data.Pitch / 2 * decodedFrame->Info.Height / 2 );
        memcpy( outFrame->data[2], decodedFrame->Data.V, decodedFrame->Data.Pitch / 2 * decodedFrame->Info.Height / 2 );
    }
    else
    {
        outFrame->data[0] = decodedFrame->Data.Y;
        outFrame->data[1] = decodedFrame->Data.U;
        outFrame->data[2] = decodedFrame->Data.V;
    }
    if( pixelFormat == PIX_FMT_NV12 )
    {
        outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
        //outFrame->linesize[1] = decodedFrame->Data.Pitch;   //uv_stride
        //outFrame->linesize[2] = 0;
        outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //uv_stride
        outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;
    }
    else if( pixelFormat == PIX_FMT_YUV420P )
    {
        outFrame->linesize[0] = decodedFrame->Data.Pitch;       //y_stride
        outFrame->linesize[1] = decodedFrame->Data.Pitch / 2;   //u_stride
        outFrame->linesize[2] = decodedFrame->Data.Pitch / 2;   //v_stride
    }

    outFrame->width = decodedFrame->Info.Width;
    outFrame->height = decodedFrame->Info.Height;
    outFrame->format = PIX_FMT_YUV420P;
    //outFrame->format = pixelFormat;
    //outFrame->key_frame = decodedFrame->Data.
    //outFrame->pts = av_rescale_q( decodedFrame->Data.TimeStamp, INTEL_MEDIA_SDK_TIMESTAMP_RESOLUTION, SRC_DATA_TMESTAMP_RESOLUTION );
    outFrame->pts = decodedFrame->Data.TimeStamp;
    outFrame->pkt_dts = decodedFrame->Data.TimeStamp;

    outFrame->display_picture_number = decodedFrame->Data.FrameOrder;
    outFrame->interlaced_frame = 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF || 
        decodedFrame->Info.PicStruct == MFX_PICSTRUCT_FIELD_REPEATED;
}


bool QuickSyncVideoDecoder::readSequenceHeader( const QnCompressedVideoDataPtr& data )
{
    SPSUnit m_sps;
    PPSUnit m_pps;



    if( data->data.size() <= 4 )
        return false;

    //memset( &m_pictureDescriptor, 0, sizeof(m_pictureDescriptor) );

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
                std::cout<<"Parsing pps\n";
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


#endif
