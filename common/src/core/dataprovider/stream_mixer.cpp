#include "stream_mixer.h"
#include <core/dataprovider/spush_media_stream_provider.h>
#include <utils/common/sleep.h>

namespace
{
    const size_t kDataQueueSize = 100;
    const quint32 kFramesBeforeReopen = 100;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnChannelMapping, (json), (originalChannel)(mappedChannels))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnResourceChannelMapping, (json), (resourceChannel)(channelMap))


QnStreamMixer::QnStreamMixer() :
    m_queue(kDataQueueSize)
{

}

QnStreamMixer::~QnStreamMixer()
{
    for (auto& source: m_sourceMap)
        source.provider->removeDataProcessor(this);

    m_sourceMap.clear();
}

void QnStreamMixer::addDataSource(QnAbstractStreamDataProviderPtr& source)
{
    auto sourceId = reinterpret_cast<uintptr_t>(source.data());

    if (!m_sourceMap.contains(sourceId))
    {
        QnProviderChannelInfo info;
        info.provider = source;

        m_sourceMap[sourceId] = info;
        source->addDataProcessor(this);
    }
}

void QnStreamMixer::removeDataSource(QnAbstractStreamDataProvider* source)
{
    auto sourceId = (uintptr_t) source;

    if (m_sourceMap.contains(sourceId))
    {
        m_sourceMap[sourceId].provider->removeDataProcessor(this);
        m_sourceMap.remove(sourceId);
    }
}

void QnStreamMixer::setUser(QnAbstractStreamDataProvider* user)
{
    m_user = user;
}


void QnStreamMixer::makeChannelMappingOperation(
    MapType type, 
    OperationType opType, 
    QnAbstractStreamDataProvider* source, 
    quint32 channelNumber, 
    quint32 mappedChannelNumber)
{
    auto handle = reinterpret_cast<uintptr_t>(source);

    if (!m_sourceMap.contains(handle))
        return;

    auto& sourceInfo = m_sourceMap[handle];

    auto& channelMap = 
        type == MapType::Video ? 
            sourceInfo.videoChannelMap : 
            sourceInfo.audioChannelMap;

    if (opType == OperationType::Insert) 
    {
        channelMap[channelNumber]
            .insert(mappedChannelNumber);


    }
    else if (opType == OperationType::Remove)
    {
        channelMap[channelNumber]
            .erase(mappedChannelNumber);
    }
    
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
    return m_queue.size() < kDataQueueSize;
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
    qDebug() << "Stream mixer's needConfigureProvider";

    if (!m_user)
        return false;

    return m_user->needConfigureProvider();
}

void QnStreamMixer::proxyOpenStream(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    qDebug() << "Proxy opening stream" << m_sourceMap.size();

    for (auto& source: m_sourceMap)
    {
        if (source.provider)
            source.provider->startIfNotRunning();
        else
            qDebug() << "Stream mixer, where is source's provider?";
    }
}

void QnStreamMixer::proxyCloseStream()
{
    qDebug() << "Proxy closing stream";
    for (auto& source: m_sourceMap)
    {
        if (source.provider)
            source.provider->pleaseStop();
    }
}

QnAbstractMediaDataPtr QnStreamMixer::retrieveData()
{
    QnAbstractDataPacketPtr data(nullptr);

    int triesLeft = 3;
    const int kWaitingTime = 100;

    while (!data && triesLeft--)
    {
        m_queue.pop(data, kWaitingTime);

        if (data)
            break;
    }

    return std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    
}

bool QnStreamMixer::isStreamOpened() const
{
    for (const auto& source: m_sourceMap)
    {
        if (!source.provider)
        {
            qDebug() << "No source provider, where is it?";
            continue;
        }

        auto mediaStreamProvider = source.provider.dynamicCast<QnAbstractMediaStreamProvider>();

        if (!mediaStreamProvider)
        {
            qDebug() << "Stream Mixer, couldn't cast to QnAbstractMediaStreamProvider";
            continue;
        }

        if (mediaStreamProvider->isStreamOpened())
        {
            qDebug() << "At least one stream is opened";
            return true;
        }
    }

    return false;
}

void QnStreamMixer::resetSources()
{
    QnMutexLocker lock(&m_mutex);   
    m_sourceMap.clear();
}

void QnStreamMixer::handlePacket(QnAbstractMediaDataPtr& data)
{
    uintptr_t provider = reinterpret_cast<uintptr_t>(data->dataProvider);
    auto originalChannel = data->channelNumber;

    if (m_sourceMap.contains(provider));
    {
        decltype(QnProviderChannelInfo::videoChannelMap)* channelMap;
        
        if(data->dataType == QnAbstractMediaData::DataType::AUDIO)
            channelMap = &m_sourceMap[provider].audioChannelMap;
        else if(data->dataType == QnAbstractMediaData::DataType::VIDEO)
            channelMap = &m_sourceMap[provider].videoChannelMap;
        else
            return;

        auto count = channelMap->count(originalChannel);
        auto size = channelMap->size();
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
                m_queue.push(dataToPush);
                isFirstIteration = false;
            }
        }    
    }
}