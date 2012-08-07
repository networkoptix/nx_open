/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICKSYNCVIDEODECODER_H
#define QUICKSYNCVIDEODECODER_H

#include <mfxvideo++.h>

#include "abstractdecoder.h"


//!Uses Intel Media SDK to provide hardware-accelerated video decoding
class QuickSyncVideoDecoder
:
    public QnAbstractVideoDecoder
{
public:
    QuickSyncVideoDecoder();
    virtual ~QuickSyncVideoDecoder();

    virtual PixelFormat GetPixelFormat();

    //!Implemenattion of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
    //!Not implemented yet
    virtual void setLightCpuMode( DecodeMode val );
    //!Implemenattion of QnAbstractVideoDecoder::getLastDecodedFrame
    virtual bool getLastDecodedFrame( CLVideoDecoderOutput* outFrame );
    //!Implemenattion of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const;
    //!Implemenattion of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const;
    //!Implemenattion of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const;
    //!Implemenattion of QnAbstractVideoDecoder::getFormat
    virtual PixelFormat getFormat() const;
    //!Drop any decoded pictures
    virtual void flush();
    //!Reset decoder. Used for seek
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    virtual void showMotion( bool show );

private:
    MFXVideoSession m_mfxSession;
    MFXVideoDECODE* m_decoder;
    mfxVideoParam m_srcStreamParam;
    mfxSyncPoint m_syncPoint;
    mfxBitstream m_inputBitStream;

    bool initDecoder( const QnCompressedVideoDataPtr& keyFrameWithSeqHeader );
    void allocateSurfacePool( const mfxFrameAllocRequest& decodingAllocRequest );
    mfxFrameSurface1* findUnlockedSurface();
    std::vector<mfxFrameSurface1> m_surfacePool;
};

#endif  //QUICKSYNCVIDEODECODER_H
