#ifndef clabstractdecoder_h_2155
#define clabstractdecoder_h_2155

#include "core/datapacket/mediadatapacket.h"
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
        INTELIPP
    };

    static QnAbstractVideoDecoder* createDecoder( const QnCompressedVideoDataPtr data, bool mtDecoding );
    static void setCodecManufacture( CLCodecManufacture codecman )
    {
        m_codecManufacture = codecman;
    }

private:
    static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractdecoder_h_2155
