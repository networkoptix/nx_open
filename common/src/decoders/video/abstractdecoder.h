#ifndef clabstractvideodecoder_h_2155
#define clabstractvideodecoder_h_2155

#include <QGLContext>

#include "core/datapacket/media_data_packet.h"
#include "utils/media/frame_info.h"


/*!
    Implementation of this class does not have to be thread-safe
*/
class QnAbstractVideoDecoder
{
public:
    // for movies: full = IPB, fast == IP only, fastest = I only
    enum DecodeMode {
        DecodeMode_NotDefined, 
        DecodeMode_Full, 
        DecodeMode_Fast, 
        DecodeMode_Fastest
    };

    explicit QnAbstractVideoDecoder();

    virtual ~QnAbstractVideoDecoder(){};

    virtual PixelFormat GetPixelFormat() const { return PIX_FMT_NONE; }
    //!Returns memory type to which decoder places decoded frames (system memory or opengl)
    virtual QnAbstractPictureData::PicStorageType targetMemoryType() const = 0;

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      * \return true If \a outFrame is filled, false if no output frame
      * \note No error information is returned!
      */
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame ) = 0;

    virtual void setLightCpuMode( DecodeMode val ) = 0;

    //!Use ulti-threaded decoding (if supported by implementation)
    virtual void setMTDecoding( bool value )
    {
        if (m_mtDecoding != value)
            m_needRecreate = true;

        m_mtDecoding = value;
    }
    //!Returns true if decoder uses multiple threads
    bool isMultiThreadedDecoding() const { return m_mtDecoding; }

    void setTryHardwareAcceleration(bool tryHardwareAcceleration);
    //!Returns true if hardware acceleration enabled
    bool isHardwareAccelerationEnabled() const;
    virtual int getWidth() const  { return 0; }
    virtual int getHeight() const { return 0; }
    virtual double getSampleAspectRatio() const { return 1; };
    virtual const AVFrame* lastFrame() const { return 0; }
    //!Reset decoder state (e.g. to reposition source stream)
    /*!
        \param data First encoded frame of new stream. It is recommended that this frame be IDR and contain sequence header
    */
    virtual void resetDecoder( QnCompressedVideoDataPtr data ) = 0;
    //!Establish picture resize during decoding
    /*!
        \param outSize Out picture size. If (0, 0) no resizing will be done
        \note This method is only a hint to a decoder. Decoder can totally ignore supplied value or can resize to a different size
    */
    virtual void setOutPictureSize( const QSize& outSize ) = 0;

private:
    QnAbstractVideoDecoder(const QnAbstractVideoDecoder&) {}
    QnAbstractVideoDecoder& operator=(const QnAbstractVideoDecoder&) { return *this; }

protected:
    bool m_tryHardwareAcceleration;
    bool m_hardwareAccelerationEnabled;

    bool m_mtDecoding;
    bool m_needRecreate;
};

class CLVideoDecoderFactory
{
public:
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
    */
    static QnAbstractVideoDecoder* createDecoder(
            const QnCompressedVideoDataPtr data,
            bool mtDecoding,
            const QGLContext* glContext = NULL );
    static void setCodecManufacture( CLCodecManufacture codecman )
    {
        m_codecManufacture = codecman;
    }

private:
    static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractvideodecoder_h_2155
