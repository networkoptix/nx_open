#ifndef clabstractvideodecoder_h_2155
#define clabstractvideodecoder_h_2155

//#include <QtOpenGL/QGLContext>

#include "core/datapacket/media_data_packet.h"
#include "core/datapacket/video_data_packet.h"
#include "utils/media/frame_info.h"


class QGLContext;

//!Abstract interface. Every video decoder MUST implement this interface
/*!
    Implementation of this class does not have to be thread-safe

    Decoder can perform picture scaling (but it is not required that decoder support this).
    To use it, call setOutPictureSize with desired output size. Decoder may ignore this value or may align value to some border and perform scaling.\n
    Methods getWidth() and getHeight() return actual picture size (after scaling).\n
    Methods getOriginalPictureSize() return picture size before any scaling but after frame cropping (if required).\n
    To find out, whether decoder supports scaling, one must check output picture size
*/
class QnAbstractVideoDecoder
{
public:
    // TODO: #Elric #enum
    // for movies: full = IPB, fast == IP only, fastest = I only
    enum DecodeMode {
        DecodeMode_NotDefined, 
        DecodeMode_Full, 
        DecodeMode_Fast, 
        DecodeMode_Fastest
    };

    // TODO: #Elric #enum
    //!Decoder capabilities
    enum DecoderCaps
    {
        //!if this not supported, decoder ignores \a setOutPictureSize call
        decodedPictureScaling = 1,
        multiThreadedMode = 2,
        //!decoder has internal frame pool and does not require to wait for decoded picture to be processed before it can start decoding next frame
        tracksDecodedPictureBuffer = 4
    };

    explicit QnAbstractVideoDecoder();

    virtual ~QnAbstractVideoDecoder() {}

    virtual PixelFormat GetPixelFormat() const { return PIX_FMT_NONE; }
    //!Returns memory type to which decoder places decoded frames (system memory or opengl)
    virtual QnAbstractPictureDataRef::PicStorageType targetMemoryType() const = 0;

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      * \param outFrame Made a pointer to pointer to allow in future return internal object to increase performance
      * \return true If \a outFrame is filled, false if no output frame
      * \note No error information is returned!
      */
    virtual bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame ) = 0;

    virtual void setLightCpuMode( DecodeMode val ) = 0;

    //!Use ulti-threaded decoding (if supported by implementation)
    virtual void setMTDecoding( bool value )
    {
        if (m_mtDecoding != value)
            m_needRecreate = true;

        m_mtDecoding = value;
    }
    //!Returns true if decoder uses multiple threads
    virtual bool isMultiThreadedDecoding() const { return m_mtDecoding; }

    void setTryHardwareAcceleration(bool tryHardwareAcceleration);
    //!Returns true if hardware acceleration enabled
    virtual bool isHardwareAccelerationEnabled() const;
    //!Returns output picture size in pixels (after scaling if it is present)
    virtual int getWidth() const  { return 0; }
    //!Returns output picture height in pixels (after scaling if it is present)
    virtual int getHeight() const { return 0; }
    //!Returns size of picture as it present in media stream (before performing scaling)
    virtual QSize getOriginalPictureSize() const = 0;
    virtual double getSampleAspectRatio() const { return 1; };
    virtual const AVFrame* lastFrame() const { return NULL; }
    //!Reset decoder state (e.g. to reposition source stream)
    /*!
        \param data First encoded frame of new stream. It is recommended that this frame be IDR and contain sequence header
    */
    virtual void resetDecoder( const QnConstCompressedVideoDataPtr& data ) = 0;
    //!Establish picture resize during decoding
    /*!
        \param outSize Out picture size. If (0, 0) no resizing will be done.
        \note This method is only a hint to a decoder. Decoder can totally ignore supplied value or can resize to a different size
        \note If decoder supports scaling it can start scaling with some delay (e.g. it can wait for next GOP before starting scaling).
            This means one cannot rely than next generated frame after this call will be scaled
    */
    virtual void setOutPictureSize( const QSize& outSize ) = 0;
    //!Get decoder capabilities
    virtual unsigned int getDecoderCaps() const = 0;
    //!Notifies decoder about source stream speed change
    virtual void setSpeed( float newValue ) = 0;

private:
    QnAbstractVideoDecoder(const QnAbstractVideoDecoder&);
    QnAbstractVideoDecoder& operator=(const QnAbstractVideoDecoder&);

protected:
    bool m_tryHardwareAcceleration;
    bool m_hardwareAccelerationEnabled;

    bool m_mtDecoding;
    bool m_needRecreate;
};

class CLVideoDecoderFactory
{
public:
    // TODO: #Elric #enum
    enum CLCodecManufacture
    {
        FFMPEG,
        INTELIPP,
        //!select most apropriate decoder (gives preference to hardware one, if available)
        AUTO
    };

    /*!
        \param data Packet of source data. MUST contain media stream sequence header
        \param mtDecoding This hint tells that decoder is allowed (not have to) to perform multi-threaded decoding
        \param glContext OpenGL context used to draw to screen. Decoder, that renders pictures directly to opengl texture,
            MUST be aware of application gl context to create textures shared with this context
        \param allowHardwareDecoding If true, method will try to find loaded hardware decoder plugin with support of requested stream type. 
            Otherwise, it will ignore any loaded decoder plugin
    */
    static QnAbstractVideoDecoder* createDecoder(
            const QnCompressedVideoDataPtr& data,
            bool mtDecoding,
            const QGLContext* glContext = NULL,
            bool allowHardwareDecoding = false );
    static void setCodecManufacture( CLCodecManufacture codecman )
    {
        m_codecManufacture = codecman;
    }

private:
    static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractvideodecoder_h_2155
