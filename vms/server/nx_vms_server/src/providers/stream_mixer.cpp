#include "stream_mixer.h"

#include <algorithm>

#include "spush_media_stream_provider.h"
#include <utils/common/sleep.h>
#include <nx/utils/log/log.h>


namespace {

static const size_t kDataQueueSize = 100;
static const quint32 kFramesBeforeReopen = 100;

} // namespace

using namespace std::literals::chrono_literals;

#ifdef ENABLE_DATA_PROVIDERS

QnStreamMixer::QnStreamMixer()
{
    NX_VERBOSE(this, "New");
}

QnStreamMixer::~QnStreamMixer()
{
    for (auto& source: m_sourceMap)
        source.provider->removeDataProcessor(this);

    m_sourceMap.clear();
    NX_VERBOSE(this, "Delete");
}

void QnStreamMixer::addDataSource(QnAbstractStreamDataProviderPtr& source)
{
    QnMutexLocker lock(&m_mutex);
    auto sourceId = reinterpret_cast<uintptr_t>(source.data());

    if (!m_sourceMap.contains(sourceId))
    {
        QnProviderChannelInfo info;
        info.provider = source;
        m_sourceMap[sourceId] = info;

        lock.unlock();
        source->addDataProcessor(this);
    }

}

void QnStreamMixer::removeDataSource(QnAbstractStreamDataProvider* source)
{
    QnMutexLocker lock(&m_mutex);
    auto sourceId = (uintptr_t) source;

    if (m_sourceMap.contains(sourceId))
    {
        m_sourceMap.remove(sourceId);

        lock.unlock();
        source->removeDataProcessor(this);
    }
}

void QnStreamMixer::setUser(QnAbstractStreamDataProvider* user)
{
    QnMutexLocker lock(&m_mutex);
    m_user = user;
}

void QnStreamMixer::makeChannelMappingOperation(
    MapType type,
    OperationType opType,
    QnAbstractStreamDataProvider* source,
    quint32 channelNumber,
    quint32 mappedChannelNumber)
{
    QnMutexLocker lock(&m_mutex);
    auto handle = reinterpret_cast<uintptr_t>(source);

    if (!m_sourceMap.contains(handle))
        return;

    auto& sourceInfo = m_sourceMap[handle];

    auto& channelMap =
        type == MapType::Video ?
            sourceInfo.videoChannelMap :
            sourceInfo.audioChannelMap;

    if (opType == OperationType::Insert)
        channelMap[channelNumber].insert(mappedChannelNumber);
    else if (opType == OperationType::Remove)
        channelMap[channelNumber].erase(mappedChannelNumber);
}

void QnStreamMixer::mapSourceVideoChannel(
    QnAbstractStreamDataProvider* source,
    quint32 videoChannelNumber,
    quint32 mappedVideoChannelNumber)
{
    makeChannelMappingOperation(
        MapType::Video,
        OperationType::Insert,
        source,
        videoChannelNumber,
        mappedVideoChannelNumber);
}

void QnStreamMixer::mapSourceAudioChannel(
    QnAbstractStreamDataProvider* source,
    quint32 audioChannelNumber,
    quint32 mappedAudioChannelNumber)
{
    makeChannelMappingOperation(
        MapType::Audio,
        OperationType::Insert,
        source,
        audioChannelNumber,
        mappedAudioChannelNumber);
}

void QnStreamMixer::unmapSourceVideoChannel(
    QnAbstractStreamDataProvider* source,
    quint32 videoChannelNumber,
    quint32 mappedVideoChannelNumber)
{
    makeChannelMappingOperation(
        MapType::Video,
        OperationType::Remove,
        source,
        videoChannelNumber,
        mappedVideoChannelNumber);
}

void QnStreamMixer::unmapSourceAudioChannel(
    QnAbstractStreamDataProvider* source,
    quint32 audioChannelNumber,
    quint32 mappedAudioChannelNumber)
{
    makeChannelMappingOperation(
        MapType::Audio,
        OperationType::Remove,
        source,
        audioChannelNumber,
        mappedAudioChannelNumber);
}

bool QnStreamMixer::canAcceptData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_buffer->canAcceptData();
}

void QnStreamMixer::putData(const QnAbstractDataPacketPtr &data)
{
    auto mediaData = std::dynamic_pointer_cast<QnAbstractMediaData>(data);

    if (!mediaData)
        return;

    handlePacket(mediaData);
}

bool QnStreamMixer::needConfigureProvider() const
{
    decltype(m_user) user;

    {
        QnMutexLocker lock(&m_mutex);
        if (!m_user)
            return false;

        user = m_user;
    }

    return user->needConfigureProvider();
}

void QnStreamMixer::proxyOpenStream(
    bool /*isCameraControlRequired*/,
    const QnLiveStreamParams& /*params*/)
{
    decltype(m_sourceMap) sourceMap;
    {
        QnMutexLocker lock(&m_mutex);
        sourceMap = m_sourceMap;
        m_buffer =
            std::make_shared<nx::streaming::MultiChannelBuffer>(
                channelCount(QnAbstractMediaData::DataType::VIDEO),
                channelCount(QnAbstractMediaData::DataType::AUDIO));
    }

    for (auto& source: sourceMap)
    {
        if (source.provider)
        {
            source.provider->stop();
            source.provider->startIfNotRunning();
        }
        else
        {
            NX_DEBUG(
                this,
                lit("StreamMixer::proxyOpenStream(), sources have no correspondent provider"));
        }
    }
}

void QnStreamMixer::proxyCloseStream()
{
    decltype(m_sourceMap) sourceMap;
    {
        QnMutexLocker lock(&m_mutex);
        sourceMap = m_sourceMap;
        if (m_buffer)
            m_buffer->terminate();
    }

    for (auto& source: sourceMap)
    {
        if (source.provider)
            source.provider->pleaseStop();
    }
}

QnAbstractMediaDataPtr QnStreamMixer::retrieveData()
{
    QnAbstractDataPacketPtr data(nullptr);
    decltype(m_buffer) buffer;
    {
        QnMutexLocker lock(&m_mutex);
        buffer = m_buffer;
    }

    if (!buffer)
        return nullptr;

    data = buffer->nextData(5000ms);
    return std::dynamic_pointer_cast<QnAbstractMediaData>(data);
}

bool QnStreamMixer::isStreamOpened() const
{
    decltype(m_sourceMap) sourceMap;
    {
        QnMutexLocker lock(&m_mutex);
        sourceMap = m_sourceMap;
    }

    for (const auto& source: sourceMap)
    {
        if (!source.provider)
        {
            NX_DEBUG(this, "No source provider");
            continue;
        }

        auto mediaStreamProvider = source.provider.dynamicCast<QnAbstractMediaStreamProvider>();

        if (!mediaStreamProvider)
        {
            NX_DEBUG(
                this,
                lit("StreamMixer::isStreamOpened(), "
                    "couldn't cast to QnAbstractMediaStreamProvider"));
            continue;
        }

        if (mediaStreamProvider->isStreamOpened())
            return true;
    }

    return false;
}

void QnStreamMixer::resetSources()
{
    decltype(m_sourceMap) sourceMap;
    {
        QnMutexLocker lock(&m_mutex);
        sourceMap.swap(m_sourceMap);
    }

    for (auto& source : sourceMap)
        source.provider->removeDataProcessor(this);
}

void QnStreamMixer::handlePacket(QnAbstractMediaDataPtr& data)
{
    QnMutexLocker lock(&m_mutex);
    uintptr_t provider = reinterpret_cast<uintptr_t>(data->dataProvider);
    auto originalChannel = data->channelNumber;

    if (m_sourceMap.contains(provider))
    {
        decltype(QnProviderChannelInfo::videoChannelMap)* channelMap;

        if(data->dataType == QnAbstractMediaData::DataType::AUDIO)
            channelMap = &m_sourceMap[provider].audioChannelMap;
        else if(data->dataType == QnAbstractMediaData::DataType::VIDEO)
            channelMap = &m_sourceMap[provider].videoChannelMap;
        else
            return;

        if (channelMap->count(originalChannel))
        {
            auto mapping = channelMap->at(originalChannel);
            bool isFirstIteration = true;
            QnAbstractMediaDataPtr dataToPush = data;

            for (const auto& mappedChannel: mapping)
            {
                if (!isFirstIteration)
                    dataToPush.reset(dataToPush->clone());

                dataToPush->channelNumber = mappedChannel;
                dataToPush->dataProvider = m_user;

                m_buffer->pushData(dataToPush);
                isFirstIteration = false;
            }
        }
    }
}

int QnStreamMixer::channelCount(QnAbstractMediaData::DataType dataType) const
{
    std::set<quint32> mappedChannels;
    for (const auto& providerInfo: m_sourceMap)
    {
        const auto& channelMap = dataType == QnAbstractMediaData::DataType::VIDEO
            ? providerInfo.videoChannelMap
            : providerInfo.audioChannelMap;

        for (const auto& entry : channelMap)
        {
            std::set<quint32> temp;
            const auto& providerMappedChannels = entry.second;
            std::set_union(
                mappedChannels.cbegin(), mappedChannels.cend(),
                providerMappedChannels.cbegin(), providerMappedChannels.cend(),
                std::inserter(temp, temp.end()));

            mappedChannels = std::move(temp);
        }
    }
    return (int) mappedChannels.size();
}

#endif // ENABLE_DATA_PROVIDERS
