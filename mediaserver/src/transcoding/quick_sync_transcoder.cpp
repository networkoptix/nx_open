/**********************************************************
* 06 aug 2012
* a.kolesnikov
***********************************************************/

#include "quick_sync_transcoder.h"


QnQuickSyncTranscoder::QnQuickSyncTranscoder(CodecID codecId):
    QnVideoTranscoder(codecId)
{
}

//!Implementation of QnCodecTranscoder::setParams
/*!
    h.264: profile, level, gop len. Default: baseline, 3.1, 25
*/
void QnQuickSyncTranscoder::setParams( const Params& params )
{
    QnCodecTranscoder::setParams( params );
}

//!Implementation of QnCodecTranscoder::setBitrate
void QnQuickSyncTranscoder::setBitrate( int value )
{
    QnCodecTranscoder::setBitrate( value );
}

void QnQuickSyncTranscoder::setSize( const QSize& size )
{
    QnVideoTranscoder::setSize( size );
}

//!Implementation of QnCodecTranscoder::transcodePacket
int QnQuickSyncTranscoder::transcodePacket( QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result)
{
    //TODO/IMPL
    return -1;
}
