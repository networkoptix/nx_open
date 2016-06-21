#pragma once

#include <core/dataprovider/stream_mixer.h>
#include <core/dataprovider/spush_media_stream_provider.h>

class QnOpteraDataProvider : public CLServerPushStreamReader
{
public: 
    QnOpteraDataProvider(const QnResourcePtr& res);
    virtual ~QnOpteraDataProvider();

protected:

    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired, 
        const QnLiveStreamParams& params) override;

    virtual bool isStreamOpened() const override;

    bool needConfigureProvider() const;

    virtual QnAbstractMediaDataPtr getNextData() override;

    virtual void closeStream() override;

    virtual void pleaseStop() override;

private:
    QnPlOnvifResourcePtr m_onvifRes;
    QnStreamMixer m_dataSource;

private:
    void initSubChannels();

    QnPlOnvifResourcePtr initSubChannelResource(quint32 channelNumber);
    QList<QnResourceChannelMapping> getVideoChannelMapping();
};

typedef std::shared_ptr<QnOpteraDataProvider> QnOpteraDataProviderPtr;