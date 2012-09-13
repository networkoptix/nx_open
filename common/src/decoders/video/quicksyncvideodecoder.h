/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICKSYNCVIDEODECODER_H
#define QUICKSYNCVIDEODECODER_H

#ifndef _DEBUG

#include <fstream>
#include <vector>

#include <mfxvideo++.h>

#include "abstractdecoder.h"

//#define WRITE_INPUT_STREAM_TO_FILE


class MyBufferAllocator
:
    public mfxBufferAllocator
{
public:
    MyBufferAllocator();

    static mfxStatus ba_alloc( mfxHDL /*pthis*/, mfxU32 nbytes, mfxU16 /*type*/, mfxMemId* mid );
    static mfxStatus ba_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxU8 **ptr );
    static mfxStatus ba_unlock(mfxHDL /*pthis*/, mfxMemId /*mid*/);
    static mfxStatus ba_free( mfxHDL /*pthis*/, mfxMemId mid );
};

class MyFrameAllocator
:
    public mfxFrameAllocator
{
public:
    MyFrameAllocator();

    static mfxStatus fa_alloc( mfxHDL /*pthis*/, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response );
    static mfxStatus fa_lock( mfxHDL /*pthis*/, mfxMemId mid, mfxFrameData* ptr );
    static mfxStatus fa_unlock( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxFrameData* ptr );
    static mfxStatus fa_gethdl( mfxHDL /*pthis*/, mfxMemId /*mid*/, mfxHDL* /*handle*/ );
    static mfxStatus fa_free( mfxHDL /*pthis*/, mfxFrameAllocResponse* response );
};


//!Uses Intel Media SDK to provide hardware-accelerated video decoding
/*!
    \note Supports only h.264 at input
    \note Not every stream decoding can be hardware accelerated. This depends on hardware model version on host system.
        One can check this by calling \a isHardwareAccelerationEnabled after media sequence header has been given to \a decode
    \note This class methods are not thread-safe
*/
class QuickSyncVideoDecoder
:
    public QnAbstractVideoDecoder
{
public:
    QuickSyncVideoDecoder();
    virtual ~QuickSyncVideoDecoder();

    virtual PixelFormat GetPixelFormat() const;

    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
    //!Not implemented yet
    virtual void setLightCpuMode( DecodeMode val );
    //!Implementation of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const;
    //!Implementation of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const;
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const;
    //!Implementation of QnAbstractVideoDecoder::lastFrame. Returned frame is valid only untile next \a decode call
    virtual const AVFrame* lastFrame() const;
    //!Reset decoder. Used for seek
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    //!Reset decoder. Used for seek
    /*!
        \note \a outSize.width is aligned to 16, \a outSize.height is aligned to 32 because of limitation of underlying API
    */
    virtual void setOutPictureSize( const QSize& outSize );

    //!introduced for test purposes
    bool decode( mfxU32 codecID, mfxBitstream* inputStream, CLVideoDecoderOutput* outFrame );

private:
    enum ProcessingState
    {
        decoding,
        processing,
        done
    };

    MFXVideoSession m_mfxSession;
    ProcessingState m_state;
    std::auto_ptr<MFXVideoDECODE> m_decoder;
    std::auto_ptr<MFXVideoVPP> m_processor;
    mfxVideoParam m_srcStreamParam;
    mfxSyncPoint m_syncPoint;
    mfxBitstream m_inputBitStream;
    bool m_initialized;
    bool m_mfxSessionEstablished;
    bool m_decodingInitialized;
    unsigned int m_totalBytesDropped;
    mfxU64 m_prevTimestamp;
    AVFrame* m_lastAVFrame;

    MyBufferAllocator m_bufAllocator;
    MyFrameAllocator m_frameAllocator;
    std::vector<mfxFrameSurface1> m_decoderSurfacePool;
    std::vector<mfxFrameSurface1> m_processingSurfacePool;
    mfxFrameAllocResponse m_decodingAllocResponse;
    mfxFrameAllocResponse m_vppAllocResponse;
    QSize m_outPictureSize;

#ifdef WRITE_INPUT_STREAM_TO_FILE
    std::ofstream m_inputStreamFile;
#endif

    bool init( mfxU32 codecID, mfxBitstream* seqHeader );
    bool initMfxSession();
    bool initDecoder( mfxU32 codecID, mfxBitstream* seqHeader );
    bool initProcessor();
    void allocateSurfacePool(
        std::vector<mfxFrameSurface1>* const surfacePool,
        mfxFrameAllocRequest* const decodingAllocRequest,
        mfxFrameAllocResponse* const allocResponse );
    mfxFrameSurface1* findUnlockedSurface( std::vector<mfxFrameSurface1>* const surfacePool );
    bool processingNeeded() const;
    void saveToAVFrame( CLVideoDecoderOutput* outFrame, mfxFrameSurface1* decodedFrame );
    bool readSequenceHeader( const QnCompressedVideoDataPtr& data );
};

#endif  //_DEBUG

#endif  //QUICKSYNCVIDEODECODER_H
