#ifdef ENABLE_ONVIF

#include "optera_data_provider.h"
#include "optera_stream_reader_resource.h"

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/onvif/onvif_stream_reader.h>
#include <utils/common/sleep.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>


namespace
{
    const int kOpteraReceiveTimout = 30;
    const int kOpteraSendTimeout = 30;
    const quint16 kStreamOpenWaitingTimeMs = 40000;
    const quint16 kSingleWaitingIterationMs = 20;
}

QnOpteraDataProvider::QnOpteraDataProvider(const QnResourcePtr& res) :
    CLServerPushStreamReader(res),
    m_onvifRes(res.dynamicCast<QnPlOnvifResource>())
{

}

QnOpteraDataProvider::~QnOpteraDataProvider()
{
    m_dataSource.proxyCloseStream();
    pleaseStop();
    wait();
}

QnAbstractMediaDataPtr QnOpteraDataProvider::getNextData()
{
    return m_dataSource.retrieveData();
}

void QnOpteraDataProvider::closeStream()
{
    qDebug() << "Closing stream";
    m_dataSource.proxyCloseStream();
    m_dataSource.setUser(nullptr);
}

bool QnOpteraDataProvider::isStreamOpened() const
{
    return  m_dataSource.isStreamOpened();
}

CameraDiagnostics::Result QnOpteraDataProvider::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    m_dataSource.resetSources();

    auto videoChannelMapping = getVideoChannelMapping();

    auto resData = qnCommon->dataPool()->data(
        m_resource.dynamicCast<QnSecurityCamResource>());

    bool doNotConfigureCamera = resData.value<bool>(
        Qn::DO_NOT_CONFIGURE_CAMERA_PARAM_NAME, false);

    for (const auto& resourceChannelMapping: videoChannelMapping)
    {
        auto resource = initSubChannelResource(
            resourceChannelMapping.resourceChannel);

        resource->setOnvifRequestsRecieveTimeout(kOpteraReceiveTimout);
        resource->setOnvifRequestsSendTimeout(kOpteraSendTimeout);

        auto reader = new QnOnvifStreamReader(resource);
        reader->setMustNotConfigureResource(doNotConfigureCamera);

        QnAbstractStreamDataProviderPtr source(reader);
        if (!doNotConfigureCamera)
            reader->setDesiredLiveParams(params);

        doNotConfigureCamera = true;
        m_dataSource.addDataSource(source);

        for (const auto& channelMapping: resourceChannelMapping.channelMap)
        {
            for(const auto& mappedChannel: channelMapping.mappedChannels)
            {
                m_dataSource.mapSourceVideoChannel(
                    source.data(),
                    channelMapping.originalChannel,
                    mappedChannel);
            }
        }
    }

    //Need to do something with audio channel mapping.
    //At this point I even don't know how Pelco's cameras transmit audio

    m_dataSource.setUser(this);
    m_dataSource.proxyOpenStream(isCameraControlRequired, params);

    auto triesLeft = kStreamOpenWaitingTimeMs / kSingleWaitingIterationMs;
    while (!m_dataSource.isStreamOpened() && triesLeft-- && !m_needStop)
        QnSleep::msleep(kSingleWaitingIterationMs);

    return CameraDiagnostics::NoErrorResult();
}

void QnOpteraDataProvider::pleaseStop()
{
    closeStream();
    m_needStop = true;
}

QnPlOnvifResourcePtr QnOpteraDataProvider::initSubChannelResource(quint32 channelNumber)
{
    QUrl url(m_onvifRes->getUrl());
    QUrl onvifUrl = url;

    qDebug() << onvifUrl;

    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(
        lit("channel"),
        QString::number(channelNumber));

    url.setQuery(urlQuery);

    QnPlOnvifResourcePtr subChannelResource(
        new nx::plugins::pelco::OpteraStreamReaderResource());

    subChannelResource->setId(QnUuid::createUuid());
    subChannelResource->setTypeId(m_onvifRes->getTypeId());
    subChannelResource->setMAC(m_onvifRes->getMAC());;

    subChannelResource->setUrl(url.toString());
    subChannelResource->setDeviceOnvifUrl(onvifUrl.toString());
    subChannelResource->setModel(m_onvifRes->getModel());
    subChannelResource->setVendor(m_onvifRes->getVendor());
    subChannelResource->setAuth(m_onvifRes->getAuth());

    subChannelResource->setName(
        QString("channel-%1").arg(channelNumber));

    return subChannelResource;
}

QList<QnResourceChannelMapping> QnOpteraDataProvider::getVideoChannelMapping()
{
    auto secRes = m_resource.dynamicCast<QnSecurityCamResource>();

    auto resData = qnCommon->dataPool()->data(
        secRes->getVendor(),
        secRes->getModel());

    return resData.value<QList<QnResourceChannelMapping>>(
        Qn::VIDEO_MULTIRESOURCE_CHANNEL_MAPPING_PARAM_NAME);
}

#endif // ENABLE_ONVIF
