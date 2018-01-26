#pragma once

#ifdef ENABLE_ONVIF

#include <core/dataprovider/stream_mixer.h>
#include <core/dataprovider/spush_media_stream_provider.h>

class QnOnvifStreamReader;

namespace nx {
namespace plugins {
namespace utils {

class MultisensorDataProvider : public CLServerPushStreamReader
{
public:
    using DataProviderFactory = std::function<QnOnvifStreamReader* (const QnResourcePtr&)>;

    MultisensorDataProvider(const QnResourcePtr& res, DataProviderFactory factory);
    virtual ~MultisensorDataProvider();

protected:

    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

    virtual bool isStreamOpened() const override;

    virtual QnAbstractMediaDataPtr getNextData() override;

    virtual void closeStream() override;

    virtual void pleaseStop() override;

private:
    QnPlOnvifResourcePtr m_onvifRes;
    QnStreamMixer m_dataSource;
    DataProviderFactory m_factory;
private:
    void initSubChannels();

    QnPlOnvifResourcePtr initSubChannelResource(quint32 channelNumber);
    QList<QnResourceChannelMapping> getVideoChannelMapping();
};


} // namespace utils
} // namespace plugins
} // namespace nx


typedef std::shared_ptr<nx::plugins::utils::MultisensorDataProvider> MultisensorDataProviderPtr;

#endif // #ifdef ENABLE_ONVIF

