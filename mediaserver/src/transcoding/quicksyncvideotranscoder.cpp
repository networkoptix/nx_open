/**********************************************************
* 06 aug 2012
* a.kolesnikov
***********************************************************/

#include "quick_sync_transcoder.h"


QnQuickSyncVideoTranscoder::QnQuickSyncVideoTranscoder( CodecID codecId )
:
    QnVideoTranscoder( codecId )
{
}

//!Implementation of QnCodecTranscoder::setParams
/*!
    h.264: profile, level, gop len. Default: baseline, 3.1, 25
*/
void QnQuickSyncVideoTranscoder::setParams( const Params& params )
{
    QnCodecTranscoder::setParams( params );
}

//!Implementation of QnCodecTranscoder::setBitrate
void QnQuickSyncVideoTranscoder::setBitrate( int value )
{
    QnCodecTranscoder::setBitrate( value );
}

void QnQuickSyncVideoTranscoder::setResolution( const QSize& value )
{
    QnVideoTranscoder::setResolution( value );
}

//!Implementation of QnCodecTranscoder::transcodePacket
int QnQuickSyncVideoTranscoder::transcodePacket( QnAbstractMediaDataPtr inputAU, QnAbstractMediaDataPtr& outputAU )
{
    //TODO/IMPL
    return -1;
}
