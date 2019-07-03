#ifdef ENABLE_ONVIF

#include <array>

#include "onvif_resource_searcher.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/network/http/http_types.h>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/dataprovider/live_stream_provider.h"

#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <common/static_common_module.h>
#include "soap_wrapper.h"
#include <onvif/soapStub.h>
#include "gsoap_async_call_wrapper.h"

#include <plugins/resource/hikvision/hikvision_resource.h>

namespace {

/*
 *  Port list used for manual camera add
 */
const static std::array<int, 4> kOnvifDeviceAltPorts =
{
    8081, //FLIR default port
    8032, // DW default port
    50080, // NEW DW cam default port
    9988 // Dahui default port
};
static const int kDefaultOnvifPort = 80;
static const std::chrono::milliseconds kManualDiscoveryConnectTimeout(5000);

} // namespace

bool hasRunningLiveProvider(QnNetworkResourcePtr netRes)
{
    bool rez = false;
    netRes->lockConsumers();
    for(QnResourceConsumer* consumer: netRes->getAllConsumers())
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (lp)
        {
            QnLongRunnable* lr = dynamic_cast<QnLongRunnable*>(lp);
            if (lr && lr->isRunning()) {
                rez = true;
                break;
            }
        }
    }

    netRes->unlockConsumers();
    return rez;
}

OnvifResourceSearcher::OnvifResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    m_informationFetcher(new OnvifResourceInformationFetcher(commonModule)),
    m_wsddSearcher(new OnvifResourceSearcherWsdd(m_informationFetcher.get()))
{
}

OnvifResourceSearcher::~OnvifResourceSearcher()
{

}

bool OnvifResourceSearcher::isProxy() const
{
    return false;
}

QString OnvifResourceSearcher::manufacture() const
{
    return QnPlOnvifResource::MANUFACTURE;
}

int OnvifResourceSearcher::autoDetectDevicePort(const QUrl& url)
{
    typedef GSoapAsyncCallWrapper <
        DeviceSoapWrapper,
        _onvifDevice__GetSystemDateAndTime,
        _onvifDevice__GetSystemDateAndTimeResponse
    > GSoapDeviceGetSystemDateAndTimeAsyncWrapper;

    QnMutex mutex;
    QnWaitCondition waitCond;
    std::vector<std::unique_ptr<GSoapDeviceGetSystemDateAndTimeAsyncWrapper>> requestList;
    int result = -1;
    int workers = kOnvifDeviceAltPorts.size();
    for (auto port: kOnvifDeviceAltPorts)
    {
        std::unique_ptr<DeviceSoapWrapper> soapWrapper(new DeviceSoapWrapper(
            lit("http://%1:%2/onvif/device_service").arg(url.host()).arg(port).toStdString(),
            /*login*/ QString(),
            /*password*/ QString(),
            /*timeDrift*/ 0));

        auto asyncWrapper = std::make_unique<GSoapDeviceGetSystemDateAndTimeAsyncWrapper>(
            std::move(soapWrapper),
            &DeviceSoapWrapper::GetSystemDateAndTime);

        _onvifDevice__GetSystemDateAndTime request;
        asyncWrapper->callAsync(
            request,
            [&, port](int soapResultCode)
            {
                QnMutexLocker lock(&mutex);
                if (soapResultCode == SOAP_OK && result == -1)
                    result = port;

                --workers;
                waitCond.wakeAll();
            });

        requestList.push_back(std::move(asyncWrapper));
    }

    QnMutexLocker lock(&mutex);
    const auto deadline = std::chrono::steady_clock::now() + kManualDiscoveryConnectTimeout;
    while (workers > 0)
    {
        const auto timeLeftMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();

        if (timeLeftMs <= 0 || !waitCond.wait(&mutex, (unsigned long) timeLeftMs))
            break;

        if (result != -1)
            return result;
    }

    return kDefaultOnvifPort;
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddr(const QUrl& _url, const QAuthenticator& auth, bool isSearchAction)
{
    QUrl url(_url);

    if( !url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    if (url.port() == -1)
        url.setPort(autoDetectDevicePort(url));

    return checkHostAddrInternal(url, auth, isSearchAction);
}

void OnvifResourceSearcher::setupResourceGroupIfNeed(const QnPlOnvifResourcePtr& resource)
{
    auto resData = qnStaticCommon
        ->dataPool()
        ->data(resource->getVendor(), resource->getModel());

    bool shouldAppearAsSingleChannel = resData.value<bool>(
        Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);

    if (!shouldAppearAsSingleChannel)
    {
        resource->setGroupId(resource->getPhysicalId());
        resource->setDefaultGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
    }
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddrInternal(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if (shouldStop())
        return QList<QnResourcePtr>();

    QString urlStr = url.toString();

    QList<QnResourcePtr> resList;
    QnResourceTypePtr typePtr = qnResTypePool->getResourceTypeByName(QLatin1String("ONVIF"));
    if (!typePtr)
        return resList;

    const int onvifPort = url.port(nx_http::DEFAULT_HTTP_PORT);
    QString onvifUrl(QLatin1String("onvif/device_service"));

    QString urlBase = urlStr.left(urlStr.indexOf(QLatin1String("?")));
    QnPlOnvifResourcePtr rpResource = resourcePool()->getResourceByUrl(urlBase).dynamicCast<QnPlOnvifResource>();

    QnPlOnvifResourcePtr resource = createResource(rpResource ? rpResource->getTypeId() : typePtr->getId(), QnResourceParams()).dynamicCast<QnPlOnvifResource>();
    resource->setDefaultAuth(auth);
    QString deviceUrl = QString(QLatin1String("http://%1:%2/%3")).arg(url.host()).arg(onvifPort).arg(onvifUrl);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    // TODO: Move out to some helper class, to remove direct link to hikvision.
    if (isSearchAction)
    {
        nx::mediaserver_core::plugins::HikvisionResource::tryToEnableIntegrationProtocols(
            deviceUrl,
            auth);
    }

    // optimization. do not pull resource every time if resource already in pool
    if (rpResource)
    {
        int channel = QUrlQuery(url.query()).queryItemValue(QLatin1String("channel")).toInt();

        if (channel == 0 && !hasRunningLiveProvider(rpResource)) {
            resource->calcTimeDrift();
            if (!resource->readDeviceInformation())
                return resList; // no answer from camera
        }
        else if (rpResource->getStatus() == Qn::Offline)
            return resList; // do not add 1..N channels if resource is offline

        resource->setPhysicalId(rpResource->getPhysicalId());
        resource->update(rpResource);
        if (channel > 0)
            resource->updateToChannel(channel-1);

        if (rpResource->getMaxChannels() > 1)
            setupResourceGroupIfNeed(resource);

        resList << resource;
        return resList;
    }

    int soapRes = 0;
    resource->calcTimeDrift(&soapRes);
    if (soapRes == SOAP_TCP_ERROR)
        return QList<QnResourcePtr>();

    if (shouldStop())
        return QList<QnResourcePtr>();

    if (resource->readDeviceInformation() && resource->getFullUrlInfo())
    {
        QString manufacturer = resource->getVendor();
        QString modelName = resource->getModel();

        auto resData = qnStaticCommon->dataPool()->data(manufacturer, modelName);
        const auto manufacturerReplacement = resData.value<QString>(
            Qn::ONVIF_MANUFACTURER_REPLACEMENT);

        if (!manufacturerReplacement.isEmpty())
        {
            manufacturer = manufacturerReplacement;
            resData = qnStaticCommon->dataPool()->data(manufacturer, modelName);
        }

        if (resource->getMAC().isNull() && resData.value<bool>(lit("isMacAddressMandatory"), true))
            return resList;

        const bool forceOnvif = QnPlOnvifResource::isCameraForcedToOnvif(manufacturer, modelName);
        if (!forceOnvif)
        {
            if (NameHelper::instance().isSupported(modelName))
                return resList;

            if (OnvifResourceInformationFetcher::ignoreCamera(manufacturer, modelName))
                return resList;

            if (OnvifResourceInformationFetcher::isModelSupported(manufacturer, modelName)) {
                qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << modelName;
                return resList;
            }
            int modelNamePos = modelName.indexOf(QLatin1String(" "));
            if (modelNamePos >= 0)
            {
                modelName = modelName.mid(modelNamePos+1);
                if (NameHelper::instance().isSupported(modelName))
                    return resList;
            }
        }

        auto manufacturerAlias = resData.value<QString>(Qn::ONVIF_VENDOR_SUBTYPE);
        manufacturer = manufacturerAlias.isEmpty() ? manufacturer : manufacturerAlias;

        QnUuid rt = m_informationFetcher->getOnvifResourceType(manufacturer, modelName);
        resource->setVendor( manufacturer );
        if (!modelName.isEmpty())
            resource->setName( modelName );
        //QnUuid rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false);
        if (!rt.isNull() && rt != resource->getTypeId())
        {
            QnPlOnvifResourcePtr updatedResource = createResource(rt, QnResourceParams()).dynamicCast<QnPlOnvifResource>();
            updatedResource->update(resource);
            updatedResource->setTypeId(rt);
            updatedResource->setPhysicalId(resource->getPhysicalId());
            updatedResource->updateOnvifUrls(resource); // runtime resource data
            updatedResource->setTimeDrift(resource->getTimeDrift()); // runtime resource data
            resource = updatedResource;
        }

        if(!resource->getUniqueId().isEmpty())
        {
            resource->fetchChannelCount();
            if (resource->getMaxChannels() > 1)
                setupResourceGroupIfNeed(resource);
            resList << resource;

            bool shouldAppearAsSingleChannel = resData
                .value<bool>(Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);

            // checking for multichannel encoders
            if (isSearchAction && !shouldAppearAsSingleChannel)
            {
                if (resource->getMaxChannels() > 1)
                {
                    resource->setGroupId(resource->getPhysicalId());
                    resource->setDefaultGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
                }

                for (int i = 1; i < resource->getMaxChannels(); ++i)
                {
                    QnPlOnvifResourcePtr res = createResource(resource->getTypeId(), QnResourceParams()).dynamicCast<QnPlOnvifResource>();
                    res->setGroupId(resource->getPhysicalId());
                    res->setDefaultGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
                    res->setVendor( manufacturer );
                    res->setPhysicalId(resource->getPhysicalId());
                    res->update(resource);
                    res->updateToChannel(i);
                    resList << res;
                }
            }
        }
    }

    return resList;
}

void OnvifResourceSearcher::pleaseStop()
{
    QnAbstractNetworkResourceSearcher::pleaseStop();
    m_wsddSearcher->pleaseStop();
}

QnResourceList OnvifResourceSearcher::findResources()
{

    QnResourceList result;

    //Order is important! mdns should be the first to avoid creating ONVIF resource, when special is expected
    //mdnsSearcher.findResources(result, specialResourceCreator);
    if (shouldStop())
         return QnResourceList();

    m_wsddSearcher->findResources( result, discoveryMode() );

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_LOG(lit("OnvifResourceSearcher::createResource: no resource type for ID = %1").arg(resourceTypeId.toString()), cl_logDEBUG1);
        return result;
    }

    //if (resourceType->getManufacture() != manufacture())
    /*
    if (!resourceType->getAllManufacturesIncludeAncessor().contains(manufacture()))
    {
        qDebug() << "OnvifResourceSearcher::createResource: manufacture " << resourceType->getManufacture()
                 << " != " << manufacture();
        return result;
    }
    */
    result = OnvifResourceInformationFetcher::createOnvifResourceByManufacture(
        resourceType->getName() == lit("ONVIF") && !params.vendor.isEmpty()
        ? params.vendor
        : resourceType->getName() ); // use name instead of manufacture to instantiate child onvif resource
    if (!result )
        return result; // not found

    result->setTypeId(resourceTypeId);

    NX_LOG(lit("OnvifResourceSearcher::createResource: create ONVIF camera resource. TypeID: %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);

    //result->deserialize(parameters);
    result->setCommonModule(commonModule());
    return result;

}

#endif //ENABLE_ONVIF
