#ifndef MEDIA_CONTEXT_H
#define MEDIA_CONTEXT_H

#include <QString>
#include <QSharedPointer>

extern "C"
{
// For struct AVCodecContext, enum CodecID.
#include <libavcodec/avcodec.h>
}

// TODO mike: Check that non-const smart pointer is not used after refactoring - QnMediaContext is immutable.
class QnMediaContext;
typedef std::shared_ptr<QnMediaContext> QnMediaContextPtr;
typedef std::shared_ptr<const QnMediaContext> QnConstMediaContextPtr;

class QnMediaContext {
public:
    QnMediaContext(const AVCodecContext* ctx);

    // TODO mike: Refactor to remove this method.
    AVCodecContext* ctx() const;

    QnMediaContext(const quint8* payload, int dataSize);
    QnMediaContext(const QByteArray& payload);
        
    QnMediaContext(CodecID codecId);
    virtual ~QnMediaContext();

    QString getCodecName() const;

    bool isSimilarTo(const QnConstMediaContextPtr& other) const;

    QnMediaContext* cloneWithoutExtradata() const;

    QByteArray serialize() const;

    // TODO mike: codec_type was obtained via ffmpeg; now should be transfered from server as a field of type enum AVMediaType.
    /**
     * Fields defined in ffmpeg's AVCodecContext.
     */
    //@{
    CodecID getCodecId() const;
    AVMediaType getCodecType() const;
    int getExtradataSize() const;
    quint8* getExtradata() const;
    int getSampleRate() const;
    int getChannels() const;
    AVSampleFormat getSampleFmt() const;
    int getBitRate() const;
    quint64 getChannelLayout() const;
    int getBlockAlign() const;
    int getBitsPerCodedSample() const;
    PixelFormat getPixFmt() const;
    int getWidth() const;
    int getHeight() const;
    int getCodedWidth() const;
    int getCodedHeight() const;
    //@}

protected:
    QnMediaContext() {}

private:
    AVCodecContext* m_ctx;
};

#endif // MEDIA_CONTEXT_H
