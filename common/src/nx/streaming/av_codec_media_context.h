#ifndef AV_CODEC_MEDIA_CONTEXT
#define AV_CODEC_MEDIA_CONTEXT

#include "media_context.h"
#include <nx/streaming/media_context_serializable_data.h>

/**
 * Implementation of QnMediaContext which wraps (and owns) AVCodecContext. Does
 * not support deserialization.
 */
class QnAvCodecMediaContext: public QnMediaContext
{
public:
    virtual ~QnAvCodecMediaContext() override;
    virtual QnAvCodecMediaContext* cloneWithoutExtradata() const override;
    virtual QByteArray serialize() const override;

    /**
     * Allocate internal AVCodecContext with ffmpeg's defaults for the codec.
     */
    QnAvCodecMediaContext(AVCodecID codecId);

    /**
     * Copy the specified AVCodecContext into the internal one.
     */
    QnAvCodecMediaContext(const AVCodecContext* context);

    /**
     * @return Pointer to the internal AVCodecContext which can be altered.
     */
    AVCodecContext* getAvCodecContext() const { return m_context; }

    /**
     * Replace existing extradata with the copy of the provided one.
     * @param extradata Can be null, in which case extradata_size is ignored.
     * @param extradata_size Can be 0.
     */
    void setExtradata(const quint8* extradata, int extradata_size);

    //--------------------------------------------------------------------------

    virtual AVCodecID getCodecId() const override;
    virtual AVMediaType getCodecType() const override;
    virtual const char* getRcEq() const override;
    virtual const quint8* getExtradata() const override;
    virtual int getExtradataSize() const override;
    virtual const quint16* getIntraMatrix() const override;
    virtual const quint16* getInterMatrix() const override;
    virtual const RcOverride* getRcOverride() const override;
    virtual int getRcOverrideCount() const override;
    virtual int getChannels() const override;
    virtual int getSampleRate() const override;
    virtual AVSampleFormat getSampleFmt() const override;
    virtual int getBitsPerCodedSample() const override;
    virtual int getCodedWidth() const override;
    virtual int getCodedHeight() const override;
    virtual int getWidth() const override;
    virtual int getHeight() const override;
    virtual int getBitRate() const override;
    virtual quint64 getChannelLayout() const override;
    virtual int getBlockAlign() const override;
    virtual int getFrameSize() const override;

private:
    QnAvCodecMediaContext(const QnAvCodecMediaContext&) /*= delete*/;
    QnAvCodecMediaContext& operator=(const QnAvCodecMediaContext&) /*= delete*/;

    AVCodecContext* m_context;
};

#endif // AV_CODEC_MEDIA_CONTEXT
