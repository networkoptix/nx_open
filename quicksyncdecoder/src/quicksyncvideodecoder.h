/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICKSYNCVIDEODECODER_H
#define QUICKSYNCVIDEODECODER_H

#include <intrin.h>

#include <QElapsedTimer>
#include <QSize>

#include <deque>
#include <fstream>
#include <vector>

#include <mfxvideo++.h>

#ifdef XVBA_TEST
#include "common.h"
#include "nalunits.h"
#else
#include <decoders/video/abstractdecoder.h>
#include <utils/media/nalunits.h>
#endif
#include "mfxallocator.h"

#define USE_D3D
//#define WRITE_INPUT_STREAM_TO_FILE_1
//#define WRITE_INPUT_STREAM_TO_FILE_2
//#define USE_ASYNC_IMPL

#ifdef XVBA_TEST
#define DISABLE_LAST_FRAME
#endif
#define DISABLE_LAST_FRAME


class PluginUsageWatcher;

//!Holds picture as DXVA surface
class D3DPictureData
:
    public QnAbstractPictureData
{
public:
    //!Implementation of QnAbstractPictureData
    virtual PicStorageType type() const;
    virtual IDirect3DSurface9* getSurface() const = 0;
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
    public QnAbstractVideoDecoder,
    public stree::AbstractResourceReader
{
public:
    /*!
        Newly created session joins session \a parentSession
    */
    QuickSyncVideoDecoder(
        MFXVideoSession* const parentSession,
        IDirect3D9Ex* direct3D9,
        const QnCompressedVideoDataPtr data,
        PluginUsageWatcher* const pluginUsageWatcher,
        unsigned int adapterNumber );
    virtual ~QuickSyncVideoDecoder();

    virtual PixelFormat GetPixelFormat() const;

    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
#ifndef XVBA_TEST
    //!Not implemented yet
    virtual void setLightCpuMode( DecodeMode val );
#endif
    //!Implementation of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const;
    //!Implementation of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const;
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const;
#ifndef XVBA_TEST
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const;
    //!Reset decoder. Used for seek
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
#endif
#ifndef DISABLE_LAST_FRAME
    //!Implementation of QnAbstractVideoDecoder::lastFrame. Returned frame is valid only until next \a decode call
    virtual const AVFrame* lastFrame() const;
#endif
    //!Hint decoder to scale decoded pictures (decoder is allowed to ignore this hint)
    /*!
        \note \a outSize.width is aligned to 16, \a outSize.height is aligned to 32 because of limitation of underlying API
    */
    virtual void setOutPictureSize( const QSize& outSize );
    //!Implementation of QnAbstractVideoDecoder::targetMemoryType
    virtual QnAbstractPictureData::PicStorageType targetMemoryType() const;

    //!Implementation of stree::AbstractResourceReader::get
    /*!
        Following parameters are supported:\n
            - framePictureWidth
            - framePictureHeight
            - framePictureSize
            - fps
            - pixelsPerSecond
            - videoMemoryUsage
    */
    virtual bool get( int resID, QVariant* const value ) const;

private:
    enum DecoderState
    {
        //!waiting for some data to start initialization
        notInitialized,
        //!in process of initialization: decoding media stream header
        decodingHeader,
        //!decoding data initialized, performing decoding...
        decoding,
        //!performing post-processing
        processing,
        //!done decoding and post-processing of current frame
        done
    };

    class SurfaceContext
    {
    public:
        mfxFrameSurface1 mfxSurface;
    };

    typedef std::vector<QSharedPointer<SurfaceContext> > SurfacePool;

#ifdef USE_ASYNC_IMPL
    enum AsyncOperationState
    {
        issDecodeInProgress,
        issWaitingVPP,
        issVPPInProgress,
        issReadyForPresentation,
        iisOutForDisplay
    };

    class AsyncOperationContext
    {
    public:
        QSharedPointer<SurfaceContext> workingSurface;
        mfxFrameSurface1* decodedFrameMfxSurf;
        QSharedPointer<SurfaceContext> decodedFrame;
        QSharedPointer<SurfaceContext> outPicture;
        mfxSyncPoint syncPoint;
        AsyncOperationState state;
        mfxStatus prevOperationResult;

        AsyncOperationContext();
    };
#endif

    //!Holds picture as QuickSync surface
    /*!
        Increases surface \a Locked counter at object initialization and decreases it during object destruction
    */
    class QuicksyncDXVAPictureData
    :
        public D3DPictureData
    {
    public:
        QuicksyncDXVAPictureData(
            AbstractMFXFrameAllocator* const allocator,
            const QSharedPointer<SurfaceContext>& surface,
            const QRect& _cropRect );
        virtual ~QuicksyncDXVAPictureData();

        //!Implementation of D3DPictureData::getSurface
        virtual IDirect3DSurface9* getSurface() const;
        //!Implementation of QnAbstractPictureData::cropRect
        /*!
            Returns crop rect from h.264 sps
        */
        virtual QRect cropRect() const;

    private:
        QSharedPointer<SurfaceContext> m_mfxSurface;
        IDirect3DSurface9* m_d3dSurface;
        const QRect m_cropRect;
    };

    MFXVideoSession* const m_parentSession;
    PluginUsageWatcher* const m_pluginUsageWatcher;
    MFXVideoSession m_mfxSession;
    DecoderState m_state;
    std::auto_ptr<MFXVideoDECODE> m_decoder;
    std::auto_ptr<MFXVideoVPP> m_processor;
    mfxVideoParam m_srcStreamParam;
    mfxSyncPoint m_syncPoint;
    mfxBitstream m_inputBitStream;
    bool m_mfxSessionEstablished;
    bool m_decodingInitialized;
    unsigned int m_totalBytesDropped;
    mfxU64 m_prevTimestamp;
#ifndef XVBA_TEST
    AVFrame* m_lastAVFrame;
#endif

    MFXBufferAllocator m_bufAllocator;
#ifdef USE_D3D
    MFXSysMemFrameAllocator m_sysMemFrameAllocator;
    MFXDirect3DSurfaceAllocator m_d3dFrameAllocator;
    MFXFrameAllocatorDispatcher m_frameAllocator;
#else
    MFXSysMemFrameAllocator m_frameAllocator;
#endif
    SurfacePool m_decoderSurfacePool;
    SurfacePool m_vppOutSurfacePool;
    size_t m_decoderSurfaceSizeInBytes;
    size_t m_vppSurfaceSizeInBytes;
    mfxFrameAllocResponse m_decodingAllocResponse;
    mfxFrameAllocResponse m_vppAllocResponse;
    QSize m_outPictureSize;
    mfxVideoParam m_processingParams;
    //!Number of lines to crop at top of decoded picture
    unsigned int m_originalFrameCropTop;
    //!Number of lines to crop at bottom of decoded picture
    unsigned int m_originalFrameCropBottom;
    //!Number of lines to crop at top of resulting picture (may differ from \a m_originalFrameCropTop because of scaling)
    unsigned int m_frameCropTopActual;
    //!Number of lines to crop at bottom of resulting picture (may differ from \a m_originalFrameCropBottom because of scaling)
    unsigned int m_frameCropBottomActual;
    bool m_reinitializeNeeded;
    std::basic_string<mfxU8> m_cachedSequenceHeader;
    double m_sourceStreamFps;
    std::deque<qint64> m_fpsCalcFrameTimestamps;
    size_t m_totalInputFrames;
    size_t m_totalOutputFrames;
    QElapsedTimer m_elapsedTimer;
    qint64 m_prevInputFrameMs;
    std::auto_ptr<SPSUnit> m_sps;
    std::auto_ptr<PPSUnit> m_pps;
    qint64 m_prevOutPictureClock;
    int m_recursionDepth;

#ifdef USE_ASYNC_IMPL
    std::vector<AsyncOperationContext> m_currentOperations;
#endif

#if defined(WRITE_INPUT_STREAM_TO_FILE_1) || defined(WRITE_INPUT_STREAM_TO_FILE_2)
    std::ofstream m_inputStreamFile;
#endif

    bool decode( mfxBitstream* const inputStream, CLVideoDecoderOutput* outFrame );
    bool init( mfxU32 codecID, mfxBitstream* seqHeader );
    bool initMfxSession();
    bool initDecoder( mfxU32 codecID, mfxBitstream* seqHeader );
    bool initProcessor();
    void allocateSurfacePool(
        SurfacePool* const surfacePool,
        mfxFrameAllocRequest* const decodingAllocRequest,
        mfxFrameAllocResponse* const allocResponse );
    void initializeSurfacePoolFromAllocResponse(
        SurfacePool* const surfacePool,
        const mfxFrameAllocResponse& allocResponse );
    void closeMFXSession();
#ifdef USE_ASYNC_IMPL
    //!Iterates all async operations and checks, whether operation state has changed
    /*!
        \param operationReadyForDisplay On return \a *operationReadyForDisplay holds operation, that composed frame, ready to be displayed, or NULL
    */
    void checkAsyncOperations( AsyncOperationContext** const operationReadyForDisplay );
#endif
    QSharedPointer<SurfaceContext> findUnlockedSurface( SurfacePool* const surfacePool );
    bool processingNeeded() const;
    void saveToAVFrame( CLVideoDecoderOutput* outFrame, const QSharedPointer<SurfaceContext>& decodedFrameCtx );
    /*!
        \return true if sequence header has been read successfully
    */
    bool readSequenceHeader( mfxBitstream* const inputStream );
    QString mfxStatusCodeToString( mfxStatus status ) const;
    QSharedPointer<SurfaceContext> getSurfaceCtxFromSurface( mfxFrameSurface1* const surf ) const;
    void resetMfxSession();
    //!Copies interleaved UV plane \a nv12UVPlane stored in USWC memory to separate U- and V- buffers in system RAM
    void deinterleaveUVPlane(
        __m128i* nv12UVPlane,
        size_t nv12UVPlaneSize,
        __m128i* yv12YPlane,
        __m128i* yv12VPlane );
};

#endif  //QUICKSYNCVIDEODECODER_H
