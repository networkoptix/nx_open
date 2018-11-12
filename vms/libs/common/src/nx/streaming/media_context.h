#ifndef MEDIA_CONTEXT_H
#define MEDIA_CONTEXT_H

#include <memory>
#include <QString>
#include <QByteArray>

extern "C"
{
#include <libavcodec/avcodec.h>
}

class QnMediaContext;
typedef std::shared_ptr<const QnMediaContext> QnConstMediaContextPtr;

/** abstract
 * Basically, an interface with getters for AVCodecContext fields which are
 * transferred from Server to Client with a media stream.
 */
class QnMediaContext
{
public:
    virtual ~QnMediaContext() = default;

    /// Clone all fields except extradata, which is set empty (null).
    virtual QnMediaContext* cloneWithoutExtradata() const = 0;

    /// NOTE: Deserialization is the responsibility of descendants.
    virtual QByteArray serialize() const = 0;

    /// Check equality of a number of fields.
    bool isSimilarTo(const QnConstMediaContextPtr& other) const;

    // Can be empty, but never null.
    QString getCodecName() const;

    // Can be empty, but never null.
    QString getAudioCodecDescription() const;

    //--------------------------------------------------------------------------
    /// Fields defined in ffmpeg's AVCodecContext.
    //@{

    virtual AVCodecID getCodecId() const = 0;
    virtual AVMediaType getCodecType() const = 0;

    /// Nul-terminated ASCII string; can be null.
    virtual const char* getRcEq() const = 0;

    /// Can be null (empty) or contain getExtradataSize() bytes.
    virtual const quint8* getExtradata() const = 0;
    virtual int getExtradataSize() const = 0;

    /// Can be null (empty) or contain QnAvCodecHelper::kMatrixLength items.
    virtual const quint16* getIntraMatrix() const = 0;

    /// Can be null (empty) or contain QnAvCodecHelper::kMatrixLength items.
    virtual const quint16* getInterMatrix() const = 0;

    /// Can be null (empty) or contain getRcOverrideCount() items.
    virtual const RcOverride* getRcOverride() const = 0;
    virtual int getRcOverrideCount() const = 0;

    virtual int getChannels() const = 0;
    virtual int getSampleRate() const = 0;
    virtual AVSampleFormat getSampleFmt() const = 0;
    virtual int getBitsPerCodedSample() const = 0;
    virtual int getCodedWidth() const = 0;
    virtual int getCodedHeight() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual int getBitRate() const = 0;
    virtual quint64 getChannelLayout() const = 0;
    virtual int getBlockAlign() const = 0;
    virtual int getFrameSize() const = 0;

    //@}
};

#endif // MEDIA_CONTEXT_H
