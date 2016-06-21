#include "basic_media_context.h"

#include <nx/streaming/media_context_serializable_data.h>

QnBasicMediaContext::QnBasicMediaContext():
    m_data(new QnMediaContextSerializableData())
{
    // Do nothing.
}

QnBasicMediaContext::~QnBasicMediaContext()
{
    // Do nothing.
}

QnBasicMediaContext* QnBasicMediaContext::cloneWithoutExtradata() const
{
    QnBasicMediaContext* newContext = new QnBasicMediaContext();

    // Copy data fields by value.
    *(newContext->m_data) = *m_data;

    newContext->m_data->extradata.clear();

    return newContext;
}

QByteArray QnBasicMediaContext::serialize() const
{
    return m_data->serialize();
}

QnBasicMediaContext* QnBasicMediaContext::deserialize(const QByteArray& data)
{
    QnBasicMediaContext* newContext = new QnBasicMediaContext();
    if (!newContext->m_data->deserialize(data))
    {
        // Serialization errors are already logged.
        return nullptr;
    }
    return newContext;
}

//-------------------------------------------------------------------------

AVCodecID QnBasicMediaContext::getCodecId() const
{
    return m_data->codecId;
}

AVMediaType QnBasicMediaContext::getCodecType() const
{
    return m_data->codecType;
}

const char* QnBasicMediaContext::getRcEq() const
{
    if (m_data->rcEq.size() == 0)
        return nullptr;
    return m_data->rcEq.constData();
}

const quint8* QnBasicMediaContext::getExtradata() const
{
    if (m_data->extradata.size() == 0)
        return nullptr;
    return (const quint8*) m_data->extradata.constData();
}

int QnBasicMediaContext::getExtradataSize() const
{
    return m_data->extradata.size();
}

const quint16* QnBasicMediaContext::getIntraMatrix() const
{
    if (m_data->intraMatrix.size() == 0)
        return nullptr;
    return m_data->intraMatrix.data();
}

const quint16* QnBasicMediaContext::getInterMatrix() const
{
    if (m_data->interMatrix.size() == 0)
        return nullptr;
    return m_data->interMatrix.data();
}

const RcOverride* QnBasicMediaContext::getRcOverride() const
{
    if (m_data->rcOverride.size() == 0)
        return nullptr;

    return m_data->rcOverride.data();
}

int QnBasicMediaContext::getRcOverrideCount() const
{
    return (int) m_data->rcOverride.size();
}

int QnBasicMediaContext::getChannels() const
{
    return m_data->channels;
}

int QnBasicMediaContext::getSampleRate() const
{
    return m_data->sampleRate;
}

AVSampleFormat QnBasicMediaContext::getSampleFmt() const
{
    return m_data->sampleFmt;
}

int QnBasicMediaContext::getBitsPerCodedSample() const
{
    return m_data->bitsPerCodedSample;
}

int QnBasicMediaContext::getCodedWidth() const
{
    return m_data->codedWidth;
}

int QnBasicMediaContext::getCodedHeight() const
{
    return m_data->codedHeight;
}

int QnBasicMediaContext::getWidth() const
{
    return m_data->width;
}

int QnBasicMediaContext::getHeight() const
{
    return m_data->height;
}

int QnBasicMediaContext::getBitRate() const
{
    return m_data->bitRate;
}

quint64 QnBasicMediaContext::getChannelLayout() const
{
    return m_data->channelLayout;
}

int QnBasicMediaContext::getBlockAlign() const
{
    return m_data->blockAlign;
}

int QnBasicMediaContext::getFrameSize() const
{
    return 0;
}
