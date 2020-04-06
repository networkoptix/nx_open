#ifndef MEDIA_CONTEXT_SERIALIZABLE_DATA_H
#define MEDIA_CONTEXT_SERIALIZABLE_DATA_H

#include <memory>
#include <vector>
#include <QByteArray>

extern "C"
{
// For declarations.
#include <libavcodec/avcodec.h>
}

/** POD
 * Contains fields of AVCodecContext (and respectively of QnMediaContext) which
 * are transferred from Server to Client with a media stream.
 *
 * Used in implementation of QnMediaContext descendants. Can be deep-copied by
 * value. Serializable via Fusion. Does not depend on ffmpeg implementation.
 */
struct QnMediaContextSerializableData
{
    QByteArray serialize() const;

    /**
     * @return Success.
     * All deserialization errors (if any) are logged, and asserted if
     * configuration is debug. On failure, fields state should be considered
     * undefined.
     */
    bool deserialize(const QByteArray& data);

    /**
     * Initialize all fields copying data from the specified AVCodecContext.
     *
     * ATTENTION: All fields are assumed to be non-initialized before the call.
     */
    void initializeFrom(const AVCodecContext* context);

    //--------------------------------------------------------------------------
    /// Fields defined in ffmpeg's AVCodecContext.
    //@{

    AVCodecID codecId;
    AVMediaType codecType;
    QByteArray rcEq; ///< Empty (represents char* null) or nul-terminated.
    QByteArray extradata;

    /// Length is 0 or QnAvCodecHelper::kMatrixLength.
    std::vector<quint16> intraMatrix;

    /// Length is 0 or QnAvCodecHelper::kMatrixLength.
    std::vector<quint16> interMatrix;

    std::vector<RcOverride> rcOverride;

    int channels;
    int sampleRate;
    AVSampleFormat sampleFmt;
    int bitsPerCodedSample;
    int codedWidth;
    int codedHeight;
    int width;
    int height;
    int bitRate;
    quint64 channelLayout;
    int blockAlign;

    //@}

private:
    // Protection against definition change of ffmpeg's struct RcOverride.
    static_assert(sizeof(RcOverride) ==
        sizeof(int) + sizeof(int) + sizeof(int) + sizeof(float),
        "Ffmpeg's struct RcOverride does not correspond to its Fusion serialization code.");
};

#endif // MEDIA_CONTEXT_SERIALIZABLE_DATA_H
