/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#include "quicksyncvideodecoder.h"


//!Maximum tries to decode frame
static const int MAX_ENCODE_ASYNC_CALL_TRIES = 3;
static const int MS_TO_WAIT_FOR_DEVICE = 5;

QuickSyncVideoDecoder::QuickSyncVideoDecoder()
:
    m_decoder( NULL )
{
    memset( &m_srcStreamParam, 0, sizeof(m_srcStreamParam) );
}

QuickSyncVideoDecoder::~QuickSyncVideoDecoder()
{
    if( m_decoder )
    {
        m_decoder->Close();
        delete m_decoder;
        m_decoder = NULL;
    }
    m_mfxSession.Close();

    //TODO/IMPL free surface pool
}

PixelFormat QuickSyncVideoDecoder::GetPixelFormat()
{
    return PIX_FMT_YUV420P;
}

//!Implemenattion of QnAbstractVideoDecoder::decode
bool QuickSyncVideoDecoder::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
    if( !m_decoder )
        if( !initDecoder( data ) )
            return false;

    //TODO/IMPL if async operation is still on the run and we got some output from previous operation, returning output...
    bool gotDisplayPicture = false;

    mfxBitstream* inputStream = NULL;
    if( data.data() )
    {
        //prepare input bitstream
        memset( &m_inputBitStream, 0, sizeof(m_inputBitStream) );
        m_inputBitStream.TimeStamp = data->timestamp;
        m_inputBitStream.Data = reinterpret_cast<mfxU8*>(data->data.data());
        m_inputBitStream.DataLength = data->data.size();
        m_inputBitStream.MaxLength = data->data.size();
        m_inputBitStream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
        inputStream = &m_inputBitStream;
    }

    mfxFrameSurface1* decodedFrame = NULL;
    for( int i = 0;
        i < MAX_ENCODE_ASYNC_CALL_TRIES;
        ++i )
    {
        mfxFrameSurface1* workingSurface = findUnlockedSurface();
        mfxStatus status = m_decoder->DecodeFrameAsync( inputStream, workingSurface, &decodedFrame, &m_syncPoint );
        switch( status )
        {
            case MFX_ERR_NONE:
                break;

            case MFX_ERR_MORE_DATA:
                return gotDisplayPicture;

            case MFX_ERR_MORE_SURFACE:
                //TODO/IMPL provide more surface space and try again...
                continue;

            case MFX_WRN_DEVICE_BUSY:
                //TODO/IMPL save error information
                Sleep( MS_TO_WAIT_FOR_DEVICE );
                continue;

            case MFX_WRN_VIDEO_PARAM_CHANGED:
                //TODO/IMPL ignoring this warning???
                return gotDisplayPicture;

            case MFX_ERR_DEVICE_LOST:
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
                //TODO/IMPL save error information
                return gotDisplayPicture;
        }
        break;
    }

    if( MFXVideoCORE_SyncOperation( m_mfxSession, m_syncPoint, INFINITE ) == MFX_ERR_NONE )
    {
        //TODO/IMPL assign output picture to outFrame
        //outFrame = decodedFrame
        gotDisplayPicture = true;
        return true;
    }

    return gotDisplayPicture;
}

//!Not implemented yet
void QuickSyncVideoDecoder::setLightCpuMode( DecodeMode val )
{
    //TODO/IMPL
}

//!Implemenattion of QnAbstractVideoDecoder::getLastDecodedFrame
bool QuickSyncVideoDecoder::getLastDecodedFrame( CLVideoDecoderOutput* outFrame )
{
    //TODO/IMPL
    return false;
}

//!Implemenattion of QnAbstractVideoDecoder::getWidth
int QuickSyncVideoDecoder::getWidth() const
{
    return m_srcStreamParam.mfx.FrameInfo.Width;
}

//!Implemenattion of QnAbstractVideoDecoder::getHeight
int QuickSyncVideoDecoder::getHeight() const
{
    return m_srcStreamParam.mfx.FrameInfo.Height;
}

//!Implemenattion of QnAbstractVideoDecoder::getSampleAspectRatio
double QuickSyncVideoDecoder::getSampleAspectRatio() const
{
    return m_srcStreamParam.mfx.FrameInfo.AspectRatioH > 0 
        ? ((double)m_srcStreamParam.mfx.FrameInfo.AspectRatioW / m_srcStreamParam.mfx.FrameInfo.AspectRatioH)
        : 0.0;
}

//!Implemenattion of QnAbstractVideoDecoder::getFormat
PixelFormat QuickSyncVideoDecoder::getFormat() const
{
    return PIX_FMT_YUV420P;
}

//!Drop any decoded picture
void QuickSyncVideoDecoder::flush()
{
    //TODO/IMPL
}

//!Reset decoder. Used for seek
void QuickSyncVideoDecoder::resetDecoder( QnCompressedVideoDataPtr data )
{
    //TODO/IMPL
}

bool QuickSyncVideoDecoder::initDecoder( const QnCompressedVideoDataPtr& keyFrameWithSeqHeader )
{
    //opening media session
    mfxVersion version;
    mfxStatus status = m_mfxSession.Init( MFX_IMPL_AUTO_ANY, &version );
    if( status != MFX_ERR_NONE )
        return false;

    //initializing decoder
    m_decoder = new MFXVideoDECODE( m_mfxSession );
    mfxBitstream streamHeader;
    memset( &streamHeader, 0, sizeof(streamHeader) );
    streamHeader.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
    streamHeader.Data = reinterpret_cast<mfxU8*>(keyFrameWithSeqHeader->data.data());
    streamHeader.DataLength = keyFrameWithSeqHeader->data.size();
    streamHeader.MaxLength = keyFrameWithSeqHeader->data.size();
    streamHeader.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    m_srcStreamParam.mfx.CodecId = 
        keyFrameWithSeqHeader->compressionType == CODEC_ID_H264
        ? MFX_CODEC_AVC
        : MFX_CODEC_MPEG2;
    status = m_decoder->DecodeHeader( &streamHeader, &m_srcStreamParam );
    if( status != MFX_ERR_NONE )
        return false;

    mfxFrameAllocRequest decodingAllocRequest;
    memset( &decodingAllocRequest, 0, sizeof(decodingAllocRequest) );
    //m_srcStreamParam.AsyncDepth = ???;
    status = m_decoder->QueryIOSurf( &m_srcStreamParam, &decodingAllocRequest );
    if( status != MFX_ERR_NONE )
        return false;

    //allocating pool of frame surfaces (decodingAllocRequest.NumFrameSuggested)
    allocateSurfacePool( decodingAllocRequest );

    m_srcStreamParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_srcStreamParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_srcStreamParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    status = m_decoder->Init( &m_srcStreamParam );
    return status == MFX_ERR_NONE;
}

void QuickSyncVideoDecoder::allocateSurfacePool( const mfxFrameAllocRequest& decodingAllocRequest )
{
    //TODO/IMPL
    m_surfacePool.resize( decodingAllocRequest.NumFrameSuggested );
    for( int i = 0; i < m_surfacePool.size(); ++i )
    {
        memset( &m_surfacePool[i], 0, sizeof(mfxFrameSurface1) );
        memcpy( &m_surfacePool[i].Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
    }
}

mfxFrameSurface1* QuickSyncVideoDecoder::findUnlockedSurface()
{
    for( int i = 0; i < m_surfacePool.size(); ++i )
    {
        if( m_surfacePool[i].Data.Locked == 0 )
            return &m_surfacePool[i];
    }
    m_surfacePool.push_back( mfxFrameSurface1() );
    memset( &m_surfacePool.back(), 0, sizeof(mfxFrameSurface1) );
    memcpy( &m_surfacePool.back().Info, &m_srcStreamParam.mfx.FrameInfo, sizeof(m_srcStreamParam.mfx.FrameInfo) );
    return &m_surfacePool.back();
}
