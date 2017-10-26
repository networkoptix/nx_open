#pragma once

#include <set>
#include <nx/fusion/model_functions.h>
#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/data_packet_queue.h>
#include "live_stream_provider.h"
struct QnChannelMapping
{
    quint32 originalChannel;
    QList<quint32> mappedChannels;
};

struct QnResourceChannelMapping
{
    quint32 resourceChannel;
    QList<QnChannelMapping> channelMap;
};

Q_DECLARE_METATYPE(QnChannelMapping)
Q_DECLARE_METATYPE(QList<QnChannelMapping>)
Q_DECLARE_METATYPE(QnResourceChannelMapping)
Q_DECLARE_METATYPE(QList<QnResourceChannelMapping>)

QN_FUSION_DECLARE_FUNCTIONS(QnChannelMapping, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnResourceChannelMapping, (json))

#ifdef ENABLE_DATA_PROVIDERS

class QnStreamMixer: public QnAbstractDataReceptor
{

    typedef std::shared_ptr<QnAbstractMediaStreamProvider> QnAbstractMediaStreamProviderPtr;

    struct QnProviderChannelInfo
    {
        QnAbstractStreamDataProviderPtr provider;
        std::map<quint32, std::set<quint32>> audioChannelMap;
        std::map<quint32, std::set<quint32>> videoChannelMap;
    };

    enum class MapType
    {
        Audio,
        Video
    };

    enum class OperationType
    {
        Insert,
        Remove
    };

public:
    QnStreamMixer();
    virtual ~QnStreamMixer();

    void addDataSource(QnAbstractStreamDataProviderPtr& source);
    void removeDataSource(QnAbstractStreamDataProvider* source);

    void setUser(QnAbstractStreamDataProvider* user);

    void mapSourceVideoChannel(
        QnAbstractStreamDataProvider* source,
        quint32 videoChannelNumber,
        quint32 mappedVideoChannelNumber);

    void mapSourceAudioChannel(
        QnAbstractStreamDataProvider* source,
        quint32 audioChannelNumber,
        quint32 mappedAudioChannelNumber);

    void unmapSourceAudioChannel(
        QnAbstractStreamDataProvider* source,
        quint32 audioChannelNumber,
        quint32 mappedAudioChannelNumber);

    void unmapSourceVideoChannel(
        QnAbstractStreamDataProvider* source,
        quint32 audioChannelNumber,
        quint32 mappedAudioChannelNumber);

    virtual bool canAcceptData() const override;
    virtual void putData( const QnAbstractDataPacketPtr& data ) override;
    virtual bool needConfigureProvider() const override;

    virtual void proxyOpenStream(bool isCameraControlRequired, const QnLiveStreamParams& params);
    virtual void proxyCloseStream();
    virtual QnAbstractMediaDataPtr retrieveData();

    bool isStreamOpened() const;

    void resetSources();

private:
    void handlePacket(QnAbstractMediaDataPtr& data);

    void makeChannelMappingOperation(
        MapType type, OperationType opType,
        QnAbstractStreamDataProvider* source,
        quint32 audioChannelNumber,
        quint32 mappedAudioChannelNumber);

private:
    mutable QnMutex m_mutex;
    QMap<uintptr_t, QnProviderChannelInfo> m_sourceMap;

    QnAbstractStreamDataProvider* m_user;
    QnDataPacketQueue m_queue;
};

#endif // ENABLE_DATA_PROVIDERS
