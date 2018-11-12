/**********************************************************
* 07 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QUICKSYNCVIDEODECODER_H
#define QUICKSYNCVIDEODECODER_H

//#define USE_OPENCL

#include <intrin.h>

#include <QSize>

#include <deque>
#include <fstream>
#include <mutex>
#include <vector>

#ifdef USE_OPENCL
#define DX9_SHARING
#define DX9_MEDIA_SHARING
#include <CL/cl.h>
#include <CL/cl_ext.h>
#endif
#include <mfxvideo++.h>

#ifdef XVBA_TEST
#include "common.h"
#include "nalunits.h"
#else
#include <plugins/videodecoders/stree/resourcecontainer.h>
#include <decoders/video/abstractdecoder.h>
#include <utils/media/nalunits.h>
#endif
#include "mfxallocator.h"
#include "mfx_video_session_ex.h"

//!if defined, scale is performed with MFX, otherwise - by directx means (by rendering to surface with scaling)
//#define SCALE_WITH_MFX
//#define WRITE_INPUT_STREAM_TO_FILE_1
//#define WRITE_INPUT_STREAM_TO_FILE_2
//#define USE_ASYNC_IMPL


class AbstractDecoderEventReceiver;
class PluginUsageWatcher;

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
#ifndef XVBA_TEST
    ,public nx::utils::stree::AbstractResourceReader
#endif
{
public:
    /*!
        Newly created session joins session \a parentSession (if not NULL)
    */
    QuickSyncVideoDecoder(
        MFXVideoSession* const parentSession,
        IDirect3DDeviceManager9* d3d9manager,
        const QnCompressedVideoDataPtr data,
        PluginUsageWatcher* const pluginUsageWatcher,
        AbstractDecoderEventReceiver* const eventReceiver,
        unsigned int adapterNumber );
    //!Object instanciation without decoder initialization
    QuickSyncVideoDecoder(
        MFXVideoSession* const parentSession,
        IDirect3DDeviceManager9* d3d9manager,
        PluginUsageWatcher* const pluginUsageWatcher,
        AbstractDecoderEventReceiver* const eventReceiver,
        unsigned int adapterNumber );
    virtual ~QuickSyncVideoDecoder();

    virtual PixelFormat GetPixelFormat() const override;

    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnConstCompressedVideoDataPtr data, QSharedPointer<CLVideoDecoderOutput>* const outFrame ) override;
#ifndef XVBA_TEST
    //!Not implemented yet
    virtual void setLightCpuMode( DecodeMode val ) override;
#endif
    //!Implementation of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const override;
    //!Implementation of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const override;
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const override;
#ifndef XVBA_TEST
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const override;
    //!Reset decoder. Used for seek
    virtual void resetDecoder( QnConstCompressedVideoDataPtr data ) override;
#endif
    //!Implementation of QnAbstractVideoDecoder::lastFrame. Returned frame is valid only until next \a decode call
    virtual const AVFrame* lastFrame() const override;
    //!Hint decoder to scale decoded pictures (decoder is allowed to ignore this hint)
    /*!
        \note \a outSize.width is aligned to 16, \a outSize.height is aligned to 32 because of limitation of underlying API
    */
    virtual void setOutPictureSize( const QSize& outSize ) override;
    //!Implementation of QnAbstractVideoDecoder::targetMemoryType
    virtual QnAbstractPictureDataRef::PicStorageType targetMemoryType() const override;
#ifndef XVBA_TEST
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    /*!
        Supports \a decodedPictureScaling and \a tracksDecodedPictureBuffer
    */
    virtual unsigned int getDecoderCaps() const override;
    //!Implementation of QnAbstractVideoDecoder::setSpeed
    virtual void setSpeed( float newValue ) override;
#endif

#ifndef XVBA_TEST
    //!Implementation of nx::utils::stree::AbstractResourceReader::get
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
#endif

    bool readSequenceHeader( const QnCompressedVideoDataPtr data, mfxVideoParam* const streamParams );
    //!Returns estimate in bytes of required frame surface memory to decode stream. Estimaion based on previously parsed header (by method \a readSequenceHeader)
    size_t estimateSurfaceMemoryUsage() const;
    /*!
        In initialization mode, decoder does not decode frames, only reads sequence header and initializes internal data, if needed
        \todo Some refactoring would be appropriate (e.g., add initialize method)
    */
    void setInitializationMode( bool val );

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
        QnAbstractPictureDataRef::SynchronizationContext syncCtx;
        bool didWeLockedSurface;

        SurfaceContext()
        :
            didWeLockedSurface( false )
        {
            memset( &mfxSurface, 0, sizeof(mfxSurface) );
        }
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
            const QRect& _cropRect,
            QuickSyncVideoDecoder* decoder );
        virtual ~QuicksyncDXVAPictureData();

        //!Implementation of D3DPictureData::getSurface
        virtual IDirect3DSurface9* getSurface() const;
        //!Implementation of QnAbstractPictureDataRef::cropRect
        /*!
            Returns crop rect from h.264 sps
        */
        virtual QRect cropRect() const;
        const QSharedPointer<SurfaceContext>& getSurfaceContext() const;

    private:
        QSharedPointer<SurfaceContext> m_mfxSurface;
        IDirect3DSurface9* m_d3dSurface;
        const QRect m_cropRect;
        QuickSyncVideoDecoder* m_decoder;
    };

    template<class T>
    class ScopedIUnknownReference
    {
    public:
        ScopedIUnknownReference( T* ptr = NULL )
        :
            m_ptr( ptr )
        {
        }

        ~ScopedIUnknownReference()
        {
            if( m_ptr )
                m_ptr->Release();
        }

        T*& get()
        {
            return m_ptr;
        }

        T* operator->() const
        {
            return m_ptr;
        }

    private:
        T* m_ptr;
    };

    typedef std::map<qint64, QnMetaDataV1Ptr> MotionInfoContainerType;

    MFXVideoSession* const m_parentSession;
    PluginUsageWatcher* const m_pluginUsageWatcher;
    AbstractDecoderEventReceiver* const m_eventReceiver;
    MFXVideoSessionEx m_mfxSession;
    DecoderState m_state;
    std::unique_ptr<MFXVideoDECODE> m_decoder;
#ifdef SCALE_WITH_MFX
    std::unique_ptr<MFXVideoVPP> m_processor;
#endif
    mfxVideoParam m_srcStreamParam;
    mfxSyncPoint m_syncPoint;
    mfxBitstream m_inputBitStream;
    bool m_mfxSessionEstablished;
    bool m_decodingInitialized;
    unsigned int m_totalBytesDropped;
    mfxU64 m_prevTimestamp;
    QSharedPointer<CLVideoDecoderOutput> m_lastAVFrame;
    mutable std::mutex m_mutex;

    MFXBufferAllocator m_bufAllocator;
    MFXSysMemFrameAllocator m_sysMemFrameAllocator;
    MFXDirect3DSurfaceAllocator m_d3dFrameAllocator;
    MFXFrameAllocatorDispatcher m_frameAllocator;
    SurfacePool m_decoderSurfacePool;
    SurfacePool m_scaledFramePool;
    size_t m_decoderSurfaceSizeInBytes;
    size_t m_scaledSurfaceSizeInBytes;
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
    DWORD m_prevInputFrameMs;
    std::unique_ptr<SPSUnit> m_sps;
    std::unique_ptr<PPSUnit> m_pps;
    DWORD m_prevOutPictureClock;
    int m_recursionDepth;
    MotionInfoContainerType m_srcMotionInfo;
    float m_speed;
    bool m_initializationMode;
    bool m_streamIsAnnexB;
    //!Length (in bytes) of "nal unit size" field in source data
    int m_nalLengthSize;
    std::basic_string<uint8_t> m_tempFrameBuffer;
    mutable CLVideoDecoderOutput* m_lastFrameInSysMem;
#ifdef USE_OPENCL
    cl_context m_clContext;
    cl_int m_prevCLStatus;
    clGetDeviceIDsFromDX9INTEL_fn m_clGetDeviceIDsFromDX9INTEL;
    clCreateFromDX9MediaSurfaceINTEL_fn m_clCreateFromDX9MediaSurfaceINTEL;
    HDC m_glContextDevice;
    HGLRC m_glContext;
#endif

#ifdef USE_ASYNC_IMPL
    std::vector<AsyncOperationContext> m_currentOperations;
#endif

#if defined(WRITE_INPUT_STREAM_TO_FILE_1) || defined(WRITE_INPUT_STREAM_TO_FILE_2)
    std::ofstream m_inputStreamFile;
#endif

    bool decode( mfxBitstream* const inputStream, QSharedPointer<CLVideoDecoderOutput>* const outFrame );
    bool init( mfxU32 codecID, mfxBitstream* seqHeader );
    bool initMfxSession();
    bool initDecoder( mfxU32 codecID, mfxBitstream* seqHeader );
#ifdef SCALE_WITH_MFX
    bool initProcessor();
#endif
#ifdef USE_OPENCL
    bool initOpenCL();
#endif
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
    void saveToAVFrame(
        CLVideoDecoderOutput* outFrame,
        const QSharedPointer<SurfaceContext>& decodedFrameCtx,
        bool returnD3DSurface,
        bool convertNV12ToYV12 = true );
    /*!
        \param streamParams If not NULL than filled with stream parameters
        \return \a MFX_ERR_NONE if sequence header has been read successfully
        \note If \a streamParams not NULL, this method will not return \a MFX_ERR_NONE until it reads fps (this can require SEI of slice header), 
            otherwise \a MFX_ERR_NONE is returned after reading sps & pps
    */
    mfxStatus readSequenceHeader( mfxBitstream* const inputStream, mfxVideoParam* const streamParams = NULL );
    QString mfxStatusCodeToString( mfxStatus status ) const;
    QSharedPointer<SurfaceContext> getSurfaceCtxFromSurface( mfxFrameSurface1* const surf ) const;
    void resetMfxSession();
#ifdef USE_OPENCL
    QString prevCLErrorText() const;
#endif
#ifndef SCALE_WITH_MFX
    void initializeScaleSurfacePool();
    mfxStatus scaleFrame( const mfxFrameSurface1& from, mfxFrameSurface1* const to );
#endif
    bool isH264SeqHeaderInExtraData( const QnConstCompressedVideoDataPtr data ) const;
    /*!
        On return, \a seqHeader contains NALUs, reader from \a data->context->ctx()->extradata
    */
    bool readH264SeqHeaderFromExtraData( const QnConstCompressedVideoDataPtr data, std::basic_string<mfxU8>* const seqHeader );

    mfxStatus doDecodingStep( mfxBitstream* const inputStream, mfxFrameSurface1** decodedFrame );
    mfxStatus doProcessingStep( QSharedPointer<SurfaceContext> decodedFrameCtx, QSharedPointer<SurfaceContext>* const scaledFrameCtx );
};

#endif  //QUICKSYNCVIDEODECODER_H
