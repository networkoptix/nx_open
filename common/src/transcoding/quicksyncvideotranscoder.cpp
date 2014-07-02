/**********************************************************
* 06 aug 2012
* a.kolesnikov
***********************************************************/

#include "quicksyncvideotranscoder.h"


#if 0

QnQuickSyncVideoTranscoder::QnQuickSyncVideoTranscoder( CodecID codecId )
:
    QnVideoTranscoder( codecId ),
    m_transcoderInitialized( false ),
    m_encoderInitialized( false ),
    m_videoProcessorInitialized( false ),
    m_decoder( NULL ),
    m_videoProcessor( NULL ),
    m_encoder( NULL )
{
}

QnQuickSyncVideoTranscoder::~QnQuickSyncVideoTranscoder()
{
    m_mfxSession.Close();

    //TODO/IMPL closing media m_mfxSession
}

//!Implementation of QnCodecTranscoder::setParams
/*!
    h.264: profile, level, gop len. Default: baseline, 3.1, 25
*/
void QnQuickSyncVideoTranscoder::setParams( const Params& params )
{
    QnCodecTranscoder::setParams( params );
    m_encoderInitialized = false;
}

//!Implementation of QnCodecTranscoder::setBitrate
void QnQuickSyncVideoTranscoder::setBitrate( int value )
{
    QnCodecTranscoder::setBitrate( value );
    m_encoderInitialized = false;
}

void QnQuickSyncVideoTranscoder::setResolution( const QSize& value )
{
    QnVideoTranscoder::setResolution( value );
    m_encoderInitialized = false;
    m_videoProcessorInitialized = false;
}

//!Implementation of QnCodecTranscoder::transcodePacket
int QnQuickSyncVideoTranscoder::transcodePacket( const QnAbstractMediaDataPtr& inputAU, QnAbstractMediaDataPtr& outputAU )
{
    if( !m_transcoderInitialized )
    {
        initializeTranscoding();
        m_encoderInitialized = true;
        m_videoProcessorInitialized = true;
        m_encoderInitialized = true;
    }

    if( !m_videoProcessorInitialized || !m_encoderInitialized )
    {
        reinitializeEncoder();
        m_videoProcessorInitialized = true;
        m_encoderInitialized = true;
    }

    //if async operation is still on the run and we got some output from previous operation, returning output...
    if( /*got some output*/ )
    {
        if( MFXVideoCORE_SyncOperation( m_mfxSession, &syncPoints.back(), 0 ) == MFX_WRN_IN_EXECUTION )
        {
            //TODO/IMPL return oldest output
            return 0;
        }
    }

    for( int i = v.size()-1; i >=0; --i )
    {
        switch( MFXVideoCORE_SyncOperation( m_mfxSession, &syncPoints[i], INFINITE ) )
        {
            case MFX_ERR_NONE:
            case MFX_ERR_MORE_SURFACE:  //more than one output is ready
                //TODO/IMPL returning oldest output
                break;

            case MFX_ERR_MORE_DATA:
                break; //need more input to produce something at output

            case MFX_ERR_ABORTED:
                //TODO/IMPL rolling up sync points of previous operations to find the one that failed...
                continue;

            case MFX_ERR_MORE_BITSTREAM:
                //TODO/IMPL allocating more output bitstream buffer
                break;

            default:
                //TODO/IMPL save error information
                return -1;
        }
        break;
    }

    // TODO: #Elric #enum
    enum State
    {
        decoding,
        converting,
        encoding,
        done
    } state = decoding;

    int curSyncPointIndex = 0;
    for( int i = 0;
        (i < MAX_ENCODE_ASYNC_CALL_TRIES*3) && (state < done);
        ++i )
    {
        mfxStatus status = MFX_ERR_NONE;
        switch( state )
        {
            case decoding:
                status = MFXVideoDECODE_DecodeFrameAsync( m_mfxSession, inputBitStream, work, vin, &syncPoints[curSyncPointIndex++] );
                break;

            case converting:
                status = MFXVideoVPP_RunFrameVPPAsync( m_mfxSession, vin, vout, &syncPoints[curSyncPointIndex++] );
                break;

            case encoding:
                status = MFXVideoENCODE_EncodeFrameAsync( m_mfxSession, NULL, vout, outputBitStream, &syncPoints[curSyncPointIndex++] );
                break;
        }

        switch( status )
        {
            case MFX_ERR_NONE:
                break;

            case MFX_ERR_MORE_DATA:
                return 0;

            case MFX_ERR_MORE_SURFACE:
                //TODO/IMPL provide more surface space and try again...
                continue;

            case MFX_ERR_NOT_ENOUGH_BUFFER:
                //TODO/IMPL save error information
                //TODO/IMPL increasing output bitstream buffer and try again
                continue;

            case MFX_WRN_DEVICE_BUSY:
                //TODO/IMPL save error information
                sleep( MS_TO_WAIT_FOR_DEVICE );
                continue;

            case MFX_WRN_VIDEO_PARAM_CHANGED:
                //TODO/IMPL ignoring this warning???
                break;

            case MFX_ERR_NOT_ENOUGH_BUFFER:
                //TODO/IMPL save error information
                //TODO/IMPL increasing output bitstream buffer size
                continue;   //trying again

            case MFX_ERR_DEVICE_LOST:
            case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            default:
                //TODO/IMPL save error information
                return -1;
        }

        //moving to the next state...
        switch( state )
        {
            case decoding:
                if( m_videoProcessor )
                {
                    state = converting;
                }
                else
                {
                    state = encoding;
                    vout = vin;
                }
                break;

            case converting:
                state = encoding;
                break;

            case encoding:
                state = done;
                break;
        }
    }

    return 0;
}

void QnQuickSyncVideoTranscoder::initializeTranscoding()
{
    //opening media session
    mfxVersion version;
    m_mfxSession.Init( MFX_IMPL_AUTO_ANY, &version );

    //initializing decoder
    m_decoder = new MFXVideoDECODE( m_mfxSession );
    mfxBitstream streamHeader;
    memset( &streamHeader, 0, sizeof(streamHeader) );
    streamHeader.Timestamp = MFX_TIMESTAMP_UNKNOWN;
    streamHeader.Data = ;
    streamHeader.DataLength = ;
    streamHeader.MaxLength = ;
    streamHeader.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    mfxVideoParam srcStreamParam;
    memset( &srcStreamParam, 0, sizeof(srcStreamParam) );
    srcStreamParam.CodecId = ;
    m_decoder->DecodeHeader( streamHeader, &srcStreamParam );

    mfxFrameAllocRequest decodingAllocRequest;
    memset( &decodingAllocRequest, 0, sizeof(decodingAllocRequest) );
    //srcStreamParam.AsyncDepth = ???;
    m_decoder->QueryIOSurf( &srcStreamParam, &decodingAllocRequest );

    m_decoder->Init( &srcStreamParam );

    //TODO/IMPL initializing video processor

    //initializing encoder

    //allocating surface pool
}

void QnQuickSyncVideoTranscoder::reinitializeEncoder()
{
    //TODO/IMPL
}

#endif
