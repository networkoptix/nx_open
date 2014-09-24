/**********************************************************
* 06 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICK_SYNC_TRANSCODER_H
#define QUICK_SYNC_TRANSCODER_H

#if 0

#include <vector>

#include <mfxvideo++.h>

#include "transcoder.h"



//!Transcodes video using Intel QuickSync API
/*!
    Supported input:\n
    - TODO
    Supported output:\n
    - h.264

    \note This class methods are not thread-safe
*/
class QnQuickSyncVideoTranscoder
:
    public QnVideoTranscoder
{
public:
    /*!
        \param codecId
    */
    QnQuickSyncVideoTranscoder( CodecID codecId );
    virtual ~QnQuickSyncVideoTranscoder();

    //!Implementation of QnCodecTranscoder::setParams
    /*!
        Supported parameters:\n
        - gopSize. 25, by default
        - deinterlace. Produce progressive frames at output. false, by default
        - qp (Quantization Parameter). TODO
        - bitrate control type. TODO

        for h.264:\n
        - profile. baseline, by default
        - level. 3.1, by default
    */
    virtual void setParams( const Params& params );
    //!Implementation of QnCodecTranscoder::setBitrate
    virtual void setBitrate( int value );
    //!Implementation of QnVideoTranscoder::setBitrate
    virtual void setResolution( const QSize& value );

    //!Implementation of QnCodecTranscoder::transcodePacket
    virtual int transcodePacket( const QnAbstractMediaDataPtr& inputAU, QnAbstractMediaDataPtr& outputAU );

private:
    bool m_transcoderInitialized;
    bool m_encoderInitialized;
    bool m_videoProcessorInitialized;
    std::vector<mfxSyncPoint> m_syncPoints;
    MFXVideoSession m_mfxSession;
    MFXVideoDECODE* m_decoder;
    MFXVideoVPP* m_videoProcessor;
    MFXVideoENCODE* m_encoder;

    void initializeTranscoding();
    void reinitializeEncoder();
};

#endif

#endif  //QUICK_SYNC_TRANSCODER_H
