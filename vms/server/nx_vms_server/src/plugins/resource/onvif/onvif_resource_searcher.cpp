#ifdef ENABLE_ONVIF

#include <array>

#include "onvif_resource_searcher.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/network/http/http_types.h>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include <providers/live_stream_provider.h>

#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
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

OnvifResourceSearcher::OnvifResourceSearcher(QnMediaServerModule* serverModule)
    :
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule),
    m_informationFetcher(new OnvifResourceInformationFetcher(serverModule)),
    m_wsddSearcher(new OnvifResourceSearcherWsdd(m_informationFetcher.get()))
{
}

OnvifResourceSearcher::~OnvifResourceSearcher()
{

}

//bool OnvifResourceSearcher::isProxy() const
//{
//    return false;
//}

QString OnvifResourceSearcher::manufacture() const
{
    return QnPlOnvifResource::MANUFACTURE;
}

int OnvifResourceSearcher::autoDetectDevicePort(const nx::utils::Url& url)
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
    int workers = (int)kOnvifDeviceAltPorts.size();
    for (auto port: kOnvifDeviceAltPorts)
    {
        std::unique_ptr<DeviceSoapWrapper> soapWrapper(new DeviceSoapWrapper(
            SoapTimeouts(serverModule()->settings().onvifTimeouts()),
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

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddr(const nx::utils::Url& _url, const QAuthenticator& auth, bool isSearchAction)
{
    nx::utils::Url url(_url);

    if( !url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    if (url.port() == -1)
        url.setPort(autoDetectDevicePort(url));

    return checkHostAddrInternal(url, auth, isSearchAction);
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddrInternal(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if (shouldStop())
        return QList<QnResourcePtr>();

    QString urlStr = url.toString();

    QList<QnResourcePtr> resList;
    QnResourceTypePtr typePtr = qnResTypePool->getResourceTypeByName(QLatin1String("ONVIF"));
    if (!typePtr)
        return resList;

    const int onvifPort = url.port(nx::network::http::DEFAULT_HTTP_PORT);
    QString onvifUrl(QLatin1String("onvif/device_service"));

    QString urlBase = urlStr.left(urlStr.indexOf(QLatin1String("?")));
    QnPlOnvifResourcePtr rpResource = serverModule()->resourcePool()
        ->getResourceByUrl(urlBase).dynamicCast<QnPlOnvifResource>();

    QnPlOnvifResourcePtr resource = createResource(rpResource ? rpResource->getTypeId() : typePtr->getId(), QnResourceParams()).dynamicCast<QnPlOnvifResource>();
    resource->setDefaultAuth(auth);
    QString deviceUrl = QString(QLatin1String("http://%1:%2/%3")).arg(url.host()).arg(onvifPort).arg(onvifUrl);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    // TODO: Move out to some helper class, to remove direct link to hikvision.
    if (isSearchAction)
    {
        nx::vms::server::plugins::HikvisionResource::tryToEnableIntegrationProtocols(
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

        auto resData = commonModule()->dataPool()
            ->data(rpResource->getVendor(), rpResource->getModel());

        if (rpResource->getMaxChannels() > 1)
        {
            resource->setGroupId(rpResource->getPhysicalId());
            resource->setDefaultGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
        }

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

        auto resData = dataPool()->data(manufacturer, modelName);
        const auto manufacturerReplacement = resData.value<QString>(
            ResourceDataKey::kOnvifManufacturerReplacement);

        if (!manufacturerReplacement.isEmpty())
        {
            manufacturer = manufacturerReplacement;
            resData = dataPool()->data(manufacturer, modelName);
        }

        if (resource->getMAC().isNull() && resData.value<bool>(lit("isMacAddressMandatory"), true))
            return resList;

        auto dataPool = commonModule()->dataPool();
        const bool forceOnvif = QnPlOnvifResource::isCameraForcedToOnvif(
            dataPool, manufacturer, modelName);
        if (!forceOnvif)
        {
            if (NameHelper::instance().isSupported(modelName))
                return resList;

            if (OnvifResourceInformationFetcher::ignoreCamera(dataPool, manufacturer, modelName))
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

        auto manufacturerAlias = resData.value<QString>(ResourceDataKey::kOnvifVendorSubtype);
        manufacturer = manufacturerAlias.isEmpty() ? manufacturer : manufacturerAlias;

        QnUuid rt = m_informationFetcher->getOnvifResourceType(manufacturer, modelName);
        resource->setVendor(manufacturer);
        if (!modelName.isEmpty())
            resource->setName(modelName);
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
            auto maxChannels = resource->getMaxChannels();
            resource->fetchChannelCount();
            //if (channel > 0)
            //    resource->updateToChannel(channel-1);
            resList << resource;

            // checking for multichannel encoders
            if (isSearchAction)
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
                    res->setVendor(manufacturer);
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

    m_wsddSearcher->findResources(result, discoveryMode());

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_DEBUG(this, lit("createResource: no resource type for ID = %1").arg(resourceTypeId.toString()));
        return result;
    }

    //if (resourceType->getManufacture() != manufacture())
    /*
    if (!resourceType->getAllManufacturesIncludeAncessor().contains(manufacture()))
    {
        qDebug() << "createResource: manufacture " << resourceType->getManufacture()
                 << " != " << manufacture();
        return result;
    }
    */
    result = OnvifResourceInformationFetcher::createOnvifResourceByManufacture(
        serverModule(),
        resourceType->getName() == lit("ONVIF") && !params.vendor.isEmpty()
        ? params.vendor
        : resourceType->getName()); // use name instead of manufacture to instantiate child onvif resource
    if (!result)
        return result; // not found

    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, lit("createResource: create ONVIF camera resource. TypeID: %1.").arg(resourceTypeId.toString()));

    //result->deserialize(parameters);
    result->setCommonModule(serverModule()->commonModule());
    return result;

}

#endif //ENABLE_ONVIF
