#ifndef frame_info_1730
#define frame_info_1730

#ifdef ENABLE_DATA_PROVIDERS

#include <atomic>

#include <QtCore/QAtomicInt>
#include <QtGui/QImage>

extern "C"
{
    #include <libavcodec/avcodec.h>
}
#include "nx/streaming/media_data_packet.h"
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>
#include "utils/color_space/image_correction.h"
#include "transcoding/filters/filter_helper.h"


#define AV_REVERSE_BLOCK_START QnAbstractMediaData::MediaFlags_ReverseBlockStart
#define AV_REVERSE_REORDERED   QnAbstractMediaData::MediaFlags_ReverseReordered


//!base class for differently-stored pictures
/*!
    Usage of synchronization context is not required
*/
class QnAbstractPictureDataRef
{
public:
    //!Used for synchronizing access to surface between decoder and renderer
    class SynchronizationContext
    {
    public:
        /*!
            QnAbstractPictureDataRef implementation MUST increment this value at object instantiation and decrement at object destruction
        */
        std::atomic<int> externalRefCounter;
        //!Sequence counter incremented by the decoder to invalidate references to the picture
        /*!
            QnAbstractPictureDataRef implementation should save sequence number at object instantiation and compare saved
            value this one to check, whether its reference is still valid
        */
        std::atomic<int> sequence;
        //!MUST be incremented for accessing picture data
        std::atomic<int> usageCounter;

        SynchronizationContext()
        :
            externalRefCounter(0),
            sequence(0),
            usageCounter(0)
        {
        }
    };

    // TODO: #Elric #enum
    enum PicStorageType
    {
        //!Picture data is stored in memory
        pstSysMemPic,
        //!Picture is stored as OpenGL texture
        pstOpenGL,
        //!Picture is presented as \a IDirect3DSurface*
        pstD3DSurface
    };

    QnAbstractPictureDataRef( SynchronizationContext* const syncCtx )
    :
        m_syncCtx( syncCtx ),
        m_initialSequence( syncCtx->sequence.load() )
    {
    }
    virtual ~QnAbstractPictureDataRef() {}

    //!Returns pic type
    virtual PicStorageType type() const = 0;
    //!Returns rect, that contains actual data
    virtual QRect cropRect() const = 0;

    /*!
        Instances of \a QnAbstractPictureDataRef MUST share \a SynchronizationContext instance of single picture
        \return Can be NULL
    */
    SynchronizationContext* syncCtx() const
    {
        return m_syncCtx;
    }
    //!If return value is \a false, access to picture data can lead to undefined behavour
    bool isValid() const
    {
        return m_syncCtx->sequence.load() == m_initialSequence;
    }

private:
    SynchronizationContext* const m_syncCtx;
    int m_initialSequence;
};

//!Picture data stored in system memory
class QnSysMemPictureData
:
    public QnAbstractPictureDataRef
{
public:
    //TODO/IMPL

    virtual QnAbstractPictureDataRef::PicStorageType type() const { return QnAbstractPictureDataRef::pstSysMemPic; }
};

#ifdef _WIN32
struct IDirect3DSurface9;

//!Holds picture as DXVA surface
class D3DPictureData
:
    public QnAbstractPictureDataRef
{
public:
    D3DPictureData( SynchronizationContext* const syncCtx )
    :
        QnAbstractPictureDataRef( syncCtx )
    {
    }

    //!Implementation of QnAbstractPictureDataRef
    virtual PicStorageType type() const { return QnAbstractPictureDataRef::pstD3DSurface; };
    virtual IDirect3DSurface9* getSurface() const = 0;
};
#endif

//!OpenGL texture (type \a pstOpenGL)
class QnOpenGLPictureData
:
    public QnAbstractPictureDataRef
{
public:
    QnOpenGLPictureData(
        SynchronizationContext* const syncCtx,
        //GLXContext _glContext,
        unsigned int _glTexture );

    virtual QnAbstractPictureDataRef::PicStorageType type() const { return QnAbstractPictureDataRef::pstOpenGL; }

    //!Returns OGL texture name
    virtual unsigned int glTexture() const;

private:
//    GLXContext m_glContext;
    unsigned int m_glTexture;
};


//!Decoded frame, ready to be rendered

class CLVideoDecoderOutput: public AVFrame
{
public:
    //!Stores picture data. If NULL, picture data is stored in \a AVFrame fields

    CLVideoDecoderOutput();
    CLVideoDecoderOutput(QImage image);
    ~CLVideoDecoderOutput();

    QImage toImage() const;

    static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
    static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
    static bool isPixelFormatSupported(AVPixelFormat format);

    void saveToFile(const char* filename);
    void clean();
    void setUseExternalData(bool value);
    bool isExternalData() const { return m_useExternalData; }
    void reallocate(int newWidth, int newHeight, int format);
    void reallocate(const QSize& size, int format);
    void reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint);
    void memZerro();

    void copyDataFrom(const AVFrame* frame);
    CLVideoDecoderOutput* rotated(int angle);
    /** Scale frame to new size */
    CLVideoDecoderOutput* scaled(const QSize& newSize, AVPixelFormat newFormat = AV_PIX_FMT_NONE);

    QSize size() const { return QSize(width, height); }

    /** Assign misc fields except but no video data */
    void assignMiscData(const CLVideoDecoderOutput* other);
    void fillRightEdge();

public:
    QSharedPointer<QnAbstractPictureDataRef> picData;

    QnAbstractMediaData::MediaFlags flags = QnAbstractMediaData::MediaFlags_None;

    /** Pixel width to pixel height ratio. Some videos have non-square pixels, we support that. */
    double sample_aspect_ratio = 0.0;

    /** Number of the video channel in video layout. */
    int channel = 0;

    FrameMetadata metadata; // addition data associated with video frame
private:
    bool m_useExternalData = false; // pointers only copied to this frame
private:
    bool invalidScaleParameters(const QSize& size) const;
    static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);

    CLVideoDecoderOutput( const CLVideoDecoderOutput& );
    const CLVideoDecoderOutput& operator=( const CLVideoDecoderOutput& );
};
typedef QSharedPointer<CLVideoDecoderOutput> CLVideoDecoderOutputPtr;
/*
struct CLVideoDecoderOutput
{
    enum downscale_factor{factor_any = 0, factor_1 = 1, factor_2 = 2, factor_4 = 4, factor_8 = 8, factor_16 = 16, factor_32 = 32, factor_64 = 64, factor_128 = 128 , factor_256 = 256};

    CLVideoDecoderOutput();
    ~CLVideoDecoderOutput();

    AVPixelFormat out_type;

    unsigned char* C1; // color components
    unsigned char* C2;
    unsigned char* C3;

    int width; // image width
    int height;// image height

    int stride1; // image width in memory of C1 component
    int stride2;
    int stride3;

    static void downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor);
    static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
    static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
    void saveToFile(const char* filename);
    void clean();
    static bool isPixelFormatSupported(AVPixelFormat format);
private:
    static void downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor8(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height);

    static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);

private:
    unsigned long m_capacity; // how many bytes are allocated ( if any )
};
*/

struct CLVideoData
{
    AVCodecID codec;

    //out frame info;
    //client needs only define ColorSpace out_type; decoder will setup ather variables
    //CLVideoDecoderOutput outFrame;

    const unsigned char* inBuffer; // pointer to compressed data
    int bufferLength; // compressed data len

    // is this frame is Intra frame. for JPEG should always be true; not nesseserally to take care about it;
    //decoder just ignores this flag
    // for user purpose only
    int keyFrame;
    bool useTwice; // some decoders delays frame by one

    int width; // image width
    int height;// image height
};

class ScreenshotInterface
{
public:
    virtual CLVideoDecoderOutputPtr getScreenshot(bool anyQuality) = 0;
    virtual QImage getGrayscaleScreenshot() = 0;
};


#endif // ENABLE_DATA_PROVIDERS

#endif //frame_info_1730

