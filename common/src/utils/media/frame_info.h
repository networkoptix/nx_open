#ifndef frame_info_1730
#define frame_info_1730

#include "libavcodec/avcodec.h"
#include "core/datapacket/mediadatapacket.h"

#define AV_REVERSE_BLOCK_START QnAbstractMediaData::MediaFlags_ReverseBlockStart
#define AV_REVERSE_REORDERED   QnAbstractMediaData::MediaFlags_ReverseReordered


//!base class for differently-stored pictures
class QnAbstractPictureData
{
public:
    enum PicStorageType
    {
        //!Picture data is stored in memory
        pstSysMemPic,
        //!Picture is stored as OpenGL texture
        pstOpenGL,
        //!Picture is presented as \a IDirect3DSurface*
        pstD3DSurface
    };

    virtual ~QnAbstractPictureData() {}
    //!Returns pic type
    virtual PicStorageType type() const = 0;
};

//!Picture data stored in system memory
class QnSysMemPictureData
:
    public QnAbstractPictureData
{
public:
    //TODO/IMPL

    virtual QnAbstractPictureData::PicStorageType type() const { return QnAbstractPictureData::pstSysMemPic; }
};

//!OpenGL texture (type \a pstOpenGL)
class QnOpenGLPictureData
:
    public QnAbstractPictureData
{
public:
    QnOpenGLPictureData(
//  		GLXContext _glContext,
   		unsigned int _glTexture );

    virtual QnAbstractPictureData::PicStorageType type() const { return QnAbstractPictureData::pstOpenGL; }

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
	QSharedPointer<QnAbstractPictureData> picData;

    CLVideoDecoderOutput();
    ~CLVideoDecoderOutput();

    static void copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst);
    static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
    void saveToFile(const char* filename);
    void clean();
    static bool isPixelFormatSupported(PixelFormat format);
    void setUseExternalData(bool value);
    bool isExternalData() const { return m_useExternalData; }
    void setDisplaying(bool value) {m_displaying = value; }
    bool isDisplaying() const { return m_displaying; }
    void reallocate(int newWidth, int newHeight, int format);
    void reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint);

public:
    int flags;

    /** Pixel width to pixel height ratio. Some videos have non-square pixels, we support that. */
    double sample_aspect_ratio; 

    /** Number of the video channel in video layout. */
    int channel;

    QnMetaDataV1Ptr metadata; // addition data associated with video frame

private:
    static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);
    void fillRightEdge();

private:
    bool m_useExternalData; // pointers only copied to this frame
    bool m_displaying;
};

/*
struct CLVideoDecoderOutput
{
    enum downscale_factor{factor_any = 0, factor_1 = 1, factor_2 = 2, factor_4 = 4, factor_8 = 8, factor_16 = 16, factor_32 = 32, factor_64 = 64, factor_128 = 128 , factor_256 = 256};

    CLVideoDecoderOutput();
    ~CLVideoDecoderOutput();

    PixelFormat out_type;

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
    static bool isPixelFormatSupported(PixelFormat format);
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
    CodecID codec; 

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

#endif //frame_info_1730

