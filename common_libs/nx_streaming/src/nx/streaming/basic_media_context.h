#ifndef BASIC_MEDIA_CONTEXT
#define BASIC_MEDIA_CONTEXT

#include <nx/streaming/media_context.h>

struct QnMediaContextSerializableData;

/**
 * Implementation of QnMediaContext which directly encapsulates field values.
 * Can be deserialized and used without depending on ffmpeg's implementation.
 */
class QnBasicMediaContext: public QnMediaContext
{
public:
    virtual ~QnBasicMediaContext() override;
    virtual QnBasicMediaContext* cloneWithoutExtradata() const override;
    virtual QByteArray serialize() const override;

    /**
     * @return A newly allocated object, or null in case of deserialization
     * errors, which are logged (if any).
     */
    static QnBasicMediaContext* deserialize(const QByteArray& data);

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
    QnBasicMediaContext();
    QnBasicMediaContext(const QnBasicMediaContext&) /*= delete*/;
    QnBasicMediaContext& operator=(const QnBasicMediaContext&) /*= delete*/;

    std::unique_ptr<QnMediaContextSerializableData> m_data;
};

#endif // BASIC_MEDIA_CONTEXT
