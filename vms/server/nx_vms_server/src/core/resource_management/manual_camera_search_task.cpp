#include "manual_camera_search_task.h"

#include <nx/utils/log/log.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/camera_driver_restriction_list.h>

namespace {

bool restrictNewManualCameraByIP(const QnSecurityCamResourcePtr& netRes)
{
    if (netRes->hasCameraCapabilities(Qn::ShareIpCapability))
        return false; //< don't block

    auto resCommonModule = netRes->commonModule();
    NX_ASSERT(resCommonModule, lit("Common module should be set for resource"));
    if (!resCommonModule)
        return true; //< Don't add resource without properly set common module

    auto resPool = resCommonModule->resourcePool();
    NX_ASSERT(resPool, "Resource should have correspondent resource pool");
    if (!resPool)
        return true; // Don't add resource without properly set resource pool

    QnNetworkResourceList existResList =
        resPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
    existResList = existResList.filtered(
        [&netRes](const QnNetworkResourcePtr& existRes)
        {
            bool sameParent = netRes->getParentId() == existRes->getParentId();
            return sameParent && existRes->getStatus() != Qn::Offline;
        });

    for (const QnNetworkResourcePtr& existRes: existResList)
    {
        QnSecurityCamResourcePtr existCam = existRes.dynamicCast<QnSecurityCamResource>();
        if (!existCam)
            continue;

        if (existCam->hasCameraCapabilities(Qn::ShareIpCapability))
            return false; //< don't block

        if (!existCam->isManuallyAdded())
            return true; //< block manual and auto camera at same IP

        if (existRes->getTypeId() != netRes->getTypeId())
        {
            // allow several manual cameras with the same IP if cameras have different ports
            QUrl url1(existRes->getUrl());
            QUrl url2(netRes->getUrl());
            if (url1.port() == url2.port())
                return true; //< camera found by different drivers with the same port
        }
    }
    return false;
}

QnManualResourceSearchEntry entryFromCamera(const QnSecurityCamResourcePtr& camera)
{
    const auto type = qnResTypePool->getResourceType(camera->getTypeId());
    const auto resourceInPool = QnResourceDiscoveryManager::findSameResource(camera);

    const bool exists =
        (resourceInPool && resourceInPool->getHostAddress() == camera->getHostAddress())
        || restrictNewManualCameraByIP(camera);

    return QnManualResourceSearchEntry(
        camera->getName(), camera->getUrl(), type->getName(),
        camera->getVendor(), camera->getUniqueId(), exists);
}

QnManualResourceSearchEntry entryFromResource(const QnResourcePtr& resource)
{
    if (const QnSecurityCamResourcePtr &camera = resource.dynamicCast<QnSecurityCamResource>())
        return entryFromCamera(camera);

    return QnManualResourceSearchEntry();
}

} // namespace


QnSearchTask::QnSearchTask(
    QnCommonModule* commonModule,
    const nx::network::SocketAddress& address,
    const QAuthenticator& auth,
    bool breakOnGotResult)
:
    m_commonModule(commonModule),
    m_auth(auth),
    m_breakIfGotResult(breakOnGotResult),
    m_blocking(false),
    m_interruptTaskProcesing(false)
{
    NX_ASSERT(commonModule);
    m_url.setAuthority(address.toString());

    NX_VERBOSE(this, "Created with URL %1", m_url);
}

void QnSearchTask::setSearchers(const SearcherList& searchers)
{
    m_searchers = searchers;
}

void QnSearchTask::setSearchDoneCallback(const SearchDoneCallback& callback)
{
    m_callback = callback;
}

void QnSearchTask::setBlocking(bool isBlocking)
{
    m_blocking = isBlocking;
}

void QnSearchTask::setInterruptTaskProcessing(bool interrupt)
{
    m_interruptTaskProcesing = interrupt;
}

bool QnSearchTask::isBlocking() const
{
    return m_blocking;
}

bool QnSearchTask::doesInterruptTaskProcessing() const
{
    return m_interruptTaskProcesing;
}

void QnSearchTask::doSearch()
{
    NX_VERBOSE(this, "Starting search on URL %1 with %2 searcher(s)", m_url, m_searchers.size());
    QnManualResourceSearchList results;
    for (const auto& checker: m_searchers)
    {
        auto seqResults = checker->checkHostAddr(m_url, m_auth, true);
        NX_VERBOSE(this, "Got %1 resources from searcher %2", seqResults.size(), checker);

        for (const auto& res: seqResults)
        {
            res->setCommonModule(checker->commonModule());
            QnSecurityCamResourcePtr camRes = res.dynamicCast<QnSecurityCamResource>();
            Q_ASSERT(camRes);
            // Checking, if found resource is reserved by some other searcher
            if(camRes && !m_commonModule->cameraDriverRestrictionList()->driverAllowedForCamera(
                checker->manufacture(),
                camRes->getVendor(),
                camRes->getModel()))
            {
                continue;   //< Camera is not allowed to be used with this driver.
            }

            results.push_back(entryFromResource(res));
        }

        if (!results.isEmpty() && m_breakIfGotResult)
            break;
    }

    NX_VERBOSE(this, "Found %1 resources on URL %2 (interrupts: %3)",
        results.size(), m_url, m_interruptTaskProcesing);
    m_callback(std::move(results), this);
}

nx::utils::Url QnSearchTask::url()
{
    return m_url;
}

QString QnSearchTask::toString()
{
    QString str;

    for (const auto& searcher: m_searchers)
        str += searcher->manufacture() + lit(" ");

    return str;
}
