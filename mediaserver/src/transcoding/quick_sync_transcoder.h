/**********************************************************
* 06 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICK_SYNC_TRANSCODER_H
#define QUICK_SYNC_TRANSCODER_H

#include "transcoder.h"


//!Transcodes video using Intel QuickSync API
/*!
    Supported input:\n
    - TODO
    Supported output:\n
    - TODO
*/
class QnQuickSyncTranscoder: public QnVideoTranscoder
{
public:
    QnQuickSyncTranscoder(CodecID codecId);

    //!Implementation of QnCodecTranscoder::setParams
    /*!
        Supported parameters:\n
        - gopSize. 25, by default
        - deinterlace. Produce progressive frames at output. false, by default
        - bitrate control type.
        - qp (Quantization Parameter).
    */
    virtual void setParams( const Params& params );
    //!Implementation of QnCodecTranscoder::setBitrate
    virtual void setBitrate( int value );
    //!Implementation of QnVideoTranscoder::setBitrate
    virtual void setResolution( const QSize& value );

    //!Implementation of QnCodecTranscoder::transcodePacket
    virtual int transcodePacket( QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result);
};

//!Transcodes video using Intel QuickSync API to h.264 format
/*!
    Supported input:\n
    - 
*/
class QnAVCQuickSyncTranscoder
:
    public QnQuickSyncTranscoder
{
public:
    //!Implementation of QnCodecTranscoder::transcodePacket
    virtual QnAbstractMediaDataPtr transcodePacket( QnAbstractMediaDataPtr media );
    //!Implementation of QnCodecTranscoder::setParams
    /*!
        Supported parameters:\n
        - profile. baseline, by default
        - level. 3.1, by default
    */
    virtual void setParams( const Params& params );
};

#endif  //QUICK_SYNC_TRANSCODER_H
