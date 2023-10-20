// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "usage_helper.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>

#include "validator.h"

namespace {

using namespace std::chrono;

static constexpr milliseconds kLicenseRefreshInterval = 5min;

QString joinedString(
    std::function<QString(Qn::LicenseType)> method, const QList<Qn::LicenseType>& licenseTypes)
{
    QStringList result;
    for (Qn::LicenseType lt: licenseTypes)
    {
        QString part = method(lt);
        if (!part.isEmpty())
            result << part;
    }

    return result.join('\n');
}

} // namespace

namespace nx::vms::license {

const QString LicenseServer::baseUrl(common::SystemContext* context)
{
    auto result = context->globalSettings()->licenseServerUrl();
    result = result.trimmed();
    while (result.endsWith('/'))
        result.chop(1);
    return result + "/nxlicensed";
}

const nx::utils::Url LicenseServer::indexUrl(common::SystemContext* context)
{
    return baseUrl(context);
}

const nx::utils::Url LicenseServer::activateUrl(common::SystemContext* context)
{
    return baseUrl(context) + "/activate.php";
}

const nx::utils::Url LicenseServer::deactivateUrl(common::SystemContext* context)
{
    return baseUrl(context) + "/api/v1/deactivate/";
}
const nx::utils::Url LicenseServer::validateUrl(common::SystemContext* context)
{
    return baseUrl(context) + "/api/v1/validate/";
}

const nx::utils::Url LicenseServer::cloudLicensesUrl(common::SystemContext* context)
{
    return baseUrl(context) + "/api/v2/license/cloud/licenses";
}

const nx::utils::Url LicenseServer::inspectUrl(common::SystemContext* context)
{
    return baseUrl(context) + "/api/v2/license/inspect/";
}

const nx::utils::Url LicenseServer::activateV2Url(common::SystemContext* context)
{
    return baseUrl(context) + "/api/v2/license/cloud/activate";
}

//#define QN_NO_LICENSE_CHECK

/**
 * Allows to use master license type instead of child type if there are not enough child licenses.
 */
struct LicenseCompatibility
{
    Qn::LicenseType master;
    Qn::LicenseType child;
};

/* Compatibility tree:
 * Trial -> Edge -> Professional -> Analog -> (VMAX, AnalogEncoder)
 * Trial -> Edge -> Professional -> Bridge
 * Trial -> IO
 * Start -> Professional -> Analog -> (VMAX, AnalogEncoder)
 * Start -> Professional -> Bridge
 * NVR -> Professional -> Analog -> (VMAX, AnalogEncoder)
 * NVR -> Professional -> Bridge
 */
static std::vector<LicenseCompatibility> compatibleLicenseType =
{
    {Qn::LC_Analog, Qn::LC_VMAX},
    {Qn::LC_Analog, Qn::LC_AnalogEncoder},

    {Qn::LC_Professional, Qn::LC_Analog},
    {Qn::LC_Professional, Qn::LC_VMAX},
    {Qn::LC_Professional, Qn::LC_AnalogEncoder},
    {Qn::LC_Professional, Qn::LC_Bridge},

    {Qn::LC_Edge, Qn::LC_Professional},
    {Qn::LC_Edge, Qn::LC_Analog},
    {Qn::LC_Edge, Qn::LC_VMAX},
    {Qn::LC_Edge, Qn::LC_AnalogEncoder},
    {Qn::LC_Edge, Qn::LC_Bridge},

    {Qn::LC_Start, Qn::LC_Professional},
    {Qn::LC_Start, Qn::LC_Analog},
    {Qn::LC_Start, Qn::LC_VMAX},
    {Qn::LC_Start, Qn::LC_AnalogEncoder},
    {Qn::LC_Start, Qn::LC_Bridge},

    {Qn::LC_Nvr, Qn::LC_Professional},
    {Qn::LC_Nvr, Qn::LC_Analog},
    {Qn::LC_Nvr, Qn::LC_VMAX},
    {Qn::LC_Nvr, Qn::LC_AnalogEncoder},
    {Qn::LC_Nvr, Qn::LC_Bridge},

    {Qn::LC_Trial, Qn::LC_Edge},
    {Qn::LC_Trial, Qn::LC_Professional},
    {Qn::LC_Trial, Qn::LC_Analog},
    {Qn::LC_Trial, Qn::LC_VMAX},
    {Qn::LC_Trial, Qn::LC_IO},
    {Qn::LC_Trial, Qn::LC_AnalogEncoder},
    {Qn::LC_Trial, Qn::LC_Bridge},

    {Qn::LC_SaasLocalRecording, Qn::LC_Edge},
    {Qn::LC_SaasLocalRecording, Qn::LC_Professional},
    {Qn::LC_SaasLocalRecording, Qn::LC_Analog},
    {Qn::LC_SaasLocalRecording, Qn::LC_VMAX},
    {Qn::LC_SaasLocalRecording, Qn::LC_IO},
    {Qn::LC_SaasLocalRecording, Qn::LC_AnalogEncoder},
    {Qn::LC_SaasLocalRecording, Qn::LC_Bridge},

};

/************************************************************************/
/* UsageHelper                                                 */
/************************************************************************/

UsageHelper::Cache::Cache()
{
    reset();
}

void UsageHelper::Cache::reset()
{
    std::fill(proposed.begin(), proposed.end(), 0);
    std::fill(total.begin(), total.end(), 0);
    for (auto& value: used)
        value.clear();
    for (auto& value: overflow)
        value.clear();
}

UsageHelper::UsageHelper(common::SystemContext* context, QObject* parent):
    base_type(parent),
    common::SystemContextAware(context),
    m_dirty(true),
    m_validator(new Validator(context))
{
    m_invalidateTimer.start(kLicenseRefreshInterval, [this]() { invalidate(); });
}

UsageHelper::~UsageHelper()
{
    m_invalidateTimer.cancelSync();
}

LicenseUsageByDevice UsageHelper::borrowLicenses(const LicenseCompatibility& compat, LicensesArrayEx& licenses) const
{
    auto borrowLicenseFromClass =
        [](LicenseUsageByDevice& srcUsed, int srcTotal, LicenseUsageByDevice& dstUsed, int dstTotal)
        {
            LicenseUsageByDevice borrowed;
            int borrowedCount = 0;
            if (dstUsed.size() > dstTotal)
            {
                int donatorRest = srcTotal - srcUsed.size();
                if (donatorRest > 0)
                {
                    borrowedCount = qMin(donatorRest, (int) dstUsed.size() - dstTotal);
                    for (int i = 0; i < borrowedCount; ++i)
                    {
                        srcUsed.push_back(dstUsed[i]);
                        borrowed.push_back(dstUsed[i]);
                    }
                    dstUsed.erase(dstUsed.begin(), dstUsed.begin() + borrowedCount);
                }
            }
            return borrowed;
        };

    return borrowLicenseFromClass(
        licenses[compat.master], m_cache.total[compat.master],
        licenses[compat.child], m_cache.total[compat.child]);
}

QString UsageHelper::getUsageText(Qn::LicenseType licenseType) const
{
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 are used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString UsageHelper::getUsageMsg() const
{
    return joinedString([this](Qn::LicenseType lt){ return getUsageText(lt); }, licenseTypes());
}

QString UsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const
{
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 will be used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString UsageHelper::getProposedUsageMsg() const
{
    return joinedString([this](Qn::LicenseType lt){ return getProposedUsageText(lt); }, licenseTypes());
}

QString UsageHelper::getRequiredText(Qn::LicenseType licenseType) const
{
    if (requiredLicenses(licenseType) > 0)
        return tr("Activate %n more %1.", "", requiredLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    if (isValid() && proposedLicenses(licenseType) > 0)
        return tr("%n more %1 will be used.", "", proposedLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    return QString();
}

QString UsageHelper::getRequiredMsg() const
{
    return joinedString([this](Qn::LicenseType lt) { return getRequiredText(lt); }, licenseTypes());
}

void UsageHelper::invalidate()
{
    m_dirty = true;
}

void UsageHelper::setCustomValidator(std::unique_ptr<Validator> validator)
{
    m_validator = std::move(validator);
}

LicensesArray toLicenseArray(const LicensesArrayEx& data)
{
    LicensesArray result;
    for (int i = 0; i < data.size(); ++i)
        result[i] = data[i].size();
    return result;
}

void UsageHelper::updateCache() const
{
    if (!m_dirty)
        return;
    /* Need to set flag right here as virtual methods may call various cache-dependent getters. */
    m_dirty = false;
    m_cache.reset();

    m_cache.licenses.update(m_context->licensePool()->getLicenses());

    /* How many licenses of each type are borrowed. */
    LicensesArrayEx borrowedLicenses;

    /* Used licenses without proposed cameras. */
    LicensesArrayEx basicUsedLicenses;

    /* Borrowed licenses count without proposed cameras. */
    LicensesArrayEx basicBorrowedLicenses;

    /* Calculate total licenses. */
    for (Qn::LicenseType lt : licenseTypes())
        m_cache.total[lt] = m_cache.licenses.totalLicenseByType(lt, m_validator.get());

    /* Calculate used licenses with and without proposed cameras (to get proposed value as difference). */
    calculateUsedLicenses(basicUsedLicenses, m_cache.used);

    /* Borrow some licenses (if available). Also repeating with and without proposed cameras. */
    for (const LicenseCompatibility& c : compatibleLicenseType)
    {
        basicBorrowedLicenses[c.child] += borrowLicenses(c, basicUsedLicenses);
        borrowedLicenses[c.child] += borrowLicenses(c, m_cache.used);
    }

    /* Finally calculating proposed and lacking (overflow) licenses. */
    for (Qn::LicenseType lt : licenseTypes())
    {
        for (int index = m_cache.total[lt]; index < m_cache.used[lt].size(); ++index)
            m_cache.overflow[lt].push_back(m_cache.used[lt][index]);
        m_cache.proposed[lt] = m_cache.used[lt].size() - basicUsedLicenses[lt].size();
    }
}

bool UsageHelper::isValid() const
{
    const auto& types = licenseTypes();
    return std::all_of(types.begin(), types.end(), [this](Qn::LicenseType lt) { return isValid(lt); });
}

bool UsageHelper::isValid(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return true;
#endif

    updateCache();
    return m_cache.overflow[licenseType].empty();
}

int UsageHelper::totalLicenses(Qn::LicenseType licenseType) const
{
    updateCache();
    return m_cache.total[licenseType];
}

int UsageHelper::usedLicenses(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return 0;
#endif

    updateCache();
    /* In all cases but analog encoder licenses m_cache.used contains already valid number. */
    if (m_cache.overflow[licenseType].empty())
        return m_cache.used[licenseType].size();
    return m_cache.total[licenseType] + m_cache.overflow[licenseType].size();
}

std::set<QnUuid> UsageHelper::usedDevices(Qn::LicenseType licenseType) const
{
    #ifdef QN_NO_LICENSE_CHECK
        return std::set<QnUuid>();
    #endif

    updateCache();
    /* In all cases but analog encoder licenses m_cache.used contains already valid number. */
    NX_ASSERT(licenseType != Qn::LC_AnalogEncoder);
    std::set<QnUuid> result;

    for (const auto& id: m_cache.used[licenseType])
        result.insert(id);
    for (const auto& id: m_cache.overflow[licenseType])
        result.insert(id);
    return result;
}

int UsageHelper::requiredLicenses(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return true;
#endif

    updateCache();
    return m_cache.overflow[licenseType].size();
}

int UsageHelper::proposedLicenses(Qn::LicenseType licenseType) const
{
    updateCache();
    return m_cache.proposed[licenseType];
}

QList<Qn::LicenseType> UsageHelper::licenseTypes() const
{
    if (m_licenseTypes.isEmpty())
        m_licenseTypes = calculateLicenseTypes();
    return m_licenseTypes;
}

/************************************************************************/
/* CamLicenseUsageHelper                                              */
/************************************************************************/
CamLicenseUsageHelper::CamLicenseUsageHelper(
    common::SystemContext* context,
    bool considerOnlineServersOnly,
    QObject* parent)
    :
    base_type(context, parent),
    m_considerOnlineServersOnly(considerOnlineServersOnly)
{
    // Watcher exists only on the client side.
    if (auto watcher = context->deviceLicenseUsageWatcher())
    {
        connect(watcher, &nx::vms::common::LicenseUsageWatcher::licenseUsageChanged, this,
            [this]()
            {
                invalidate();
                emit licenseUsageChanged();
            });
    }
}

CamLicenseUsageHelper::CamLicenseUsageHelper(
    const QnVirtualCameraResourceList& proposedCameras,
    bool proposedEnable,
    common::SystemContext* context,
    QObject* parent)
    :
    CamLicenseUsageHelper(context, parent)
{
    propose(proposedCameras, proposedEnable);
}

CamLicenseUsageHelper::CamLicenseUsageHelper(
    const QnVirtualCameraResourcePtr& proposedCamera,
    bool proposedEnable,
    common::SystemContext* context,
    QObject* parent)
:
    CamLicenseUsageHelper(context, parent)
{
    propose(proposedCamera, proposedEnable);
}

void CamLicenseUsageHelper::propose(
    const QnVirtualCameraResourcePtr& proposedCamera,
    bool proposedEnable)
{
    return propose(QnVirtualCameraResourceList() << proposedCamera, proposedEnable);
}

void CamLicenseUsageHelper::propose(
    const QnVirtualCameraResourceList& proposedCameras,
    bool proposedEnable)
{
    const auto& proposedCamerasSet = QSet(proposedCameras.begin(), proposedCameras.end());
    if (proposedEnable)
    {
        m_proposedToEnable.unite(proposedCamerasSet);
        m_proposedToDisable.subtract(proposedCamerasSet);
    }
    else
    {
        m_proposedToDisable.unite(proposedCamerasSet);
        m_proposedToEnable.subtract(proposedCamerasSet);
    }
    invalidate();
}

bool CamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    return isOverflowForCamera(camera, camera->isScheduleEnabled());
}

bool CamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr& camera,
    bool cachedLicenseUsed) const
{
    bool requiresLicense = cachedLicenseUsed;
    requiresLicense &= !m_proposedToDisable.contains(camera);
    requiresLicense |= m_proposedToEnable.contains(camera);
    return requiresLicense && !isValid(camera->licenseType());
}

bool CamLicenseUsageHelper::canEnableRecording(
    const QnVirtualCameraResourcePtr& proposedCamera)
{
    return canEnableRecording(QnVirtualCameraResourceList() << proposedCamera);
}

bool CamLicenseUsageHelper::canEnableRecording(
    const QnVirtualCameraResourceList& proposedCameras)
{
    const auto allLicenseTypes = calculateLicenseTypes();
    std::array<bool, Qn::LC_Count> validTypes;
    for (auto licenseType: allLicenseTypes)
        validTypes[licenseType] = isValid(licenseType);

    const auto proposedBackup = m_proposedToEnable;

    QSet<Qn::LicenseType> camerasLicenses;
    for (const auto& camera: proposedCameras)
    {
        if (camera->isScheduleEnabled())
            continue;

        camerasLicenses.insert(camera->licenseType());
        m_proposedToEnable.insert(camera);
    }
    invalidate();

    const bool result = std::all_of(allLicenseTypes.cbegin(), allLicenseTypes.cend(),
        [&](Qn::LicenseType licenseType)
        {
            if (camerasLicenses.contains(licenseType))
                return isValid(licenseType);

            return !validTypes[licenseType] || isValid(licenseType);
        });

    m_proposedToEnable = proposedBackup;
    invalidate();

    return result;
}

QList<Qn::LicenseType> CamLicenseUsageHelper::calculateLicenseTypes() const
{
    return QList<Qn::LicenseType>()
        << Qn::LC_Trial
        << Qn::LC_Analog    // both places
        << Qn::LC_Professional
        << Qn::LC_Edge
        << Qn::LC_VMAX  //only main page
        << Qn::LC_Bridge
        << Qn::LC_AnalogEncoder
        << Qn::LC_IO
        << Qn::LC_Start
        << Qn::LC_Nvr
        << Qn::LC_SaasLocalRecording
        ;
}

void CamLicenseUsageHelper::calculateUsedLicenses(
    LicensesArrayEx& basicUsedLicenses,
    LicensesArrayEx& proposedToUse) const
{
    auto groupId = [](const QnSecurityCamResourcePtr& camera)
    {
        return camera->isSharingLicenseInGroup()
            ? camera->getGroupId()
            : camera->getId().toString();
    };

    QMap<QString, QSet<QnSecurityCamResourcePtr>> oldCameras;
    for (const auto& camera: resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        if (camera->isScheduleEnabled())
        {
            if (m_considerOnlineServersOnly)
            {
                auto server = camera->getParentResource();
                if (server && !server->isOnline())
                    continue;
            }
            oldCameras[groupId(camera)].insert(camera);
        }
    }

    for (const auto& data : oldCameras)
    {
        const auto& c = *data.begin();
        basicUsedLicenses[c->licenseType()].push_back(c->getId());
    }

    auto newCameras = oldCameras;
    for (const auto& camera: m_proposedToEnable)
        newCameras[groupId(camera)].insert(camera);
    for (const auto& camera: m_proposedToDisable)
        newCameras[groupId(camera)].remove(camera);

    for (const auto& data: newCameras)
    {
        if (!data.isEmpty())
        {
            const auto& c = *data.begin();
            proposedToUse[c->licenseType()].push_back(c->getId());
        }
    }

}

//////////////////////////////////////////////////////////////////////////

SingleCamLicenseStatusHelper::SingleCamLicenseStatusHelper(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    base_type(parent),
    m_camera(camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return;

    m_helper.reset(new CamLicenseUsageHelper(camera, true, camera->systemContext()));
    connect(m_helper.get(), &CamLicenseUsageHelper::licenseUsageChanged,
        this, &SingleCamLicenseStatusHelper::licenseStatusChanged);
}

SingleCamLicenseStatusHelper::~SingleCamLicenseStatusHelper()
{
    if (!m_camera)
        return;

    m_camera->disconnect(this);
    m_helper->disconnect(this);
}

UsageStatus SingleCamLicenseStatusHelper::status() const
{
    if (!m_camera)
        return UsageStatus::invalid;

    const bool isLicenseUsed = m_camera->isScheduleEnabled();
    if (m_helper->isOverflowForCamera(m_camera, isLicenseUsed))
        return UsageStatus::overflow;

    return (isLicenseUsed ? UsageStatus::used : UsageStatus::notUsed);
}

/************************************************************************/
/* VideoWallLicenseUsageHelper                                        */
/************************************************************************/
VideoWallLicenseUsageHelper::VideoWallLicenseUsageHelper(
    common::SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent)
{
    // Watcher exists only on the client side.
    if (auto watcher = context->videoWallLicenseUsageWatcher())
    {
        connect(watcher, &nx::vms::common::LicenseUsageWatcher::licenseUsageChanged, this,
            &UsageHelper::invalidate);
    }
}

bool VideoWallLicenseUsageHelper::isValid() const
{
    const auto state = systemContext()->saasServiceManager()->saasState();
    if (state != nx::vms::api::SaasState::uninitialized)
        return state == nx::vms::api::SaasState::active;
    return base_type::isValid();
}

bool VideoWallLicenseUsageHelper::isValid(Qn::LicenseType licenseType) const
{
    const auto state = systemContext()->saasServiceManager()->saasState();
    if (state != nx::vms::api::SaasState::uninitialized)
        return state == nx::vms::api::SaasState::active;
    return base_type::isValid(licenseType);
}

QList<Qn::LicenseType> VideoWallLicenseUsageHelper::calculateLicenseTypes() const
{
    return QList<Qn::LicenseType>() << Qn::LC_VideoWall;
}

void VideoWallLicenseUsageHelper::calculateUsedLicenses(
    LicensesArrayEx& basicUsedLicenses,
    LicensesArrayEx& proposedToUse) const
{
    std::set<QnUuid> usedScreens;

    // Calculating total screens.
    for (const auto& videowall: resourcePool()->getResources<QnVideoWallResource>())
    {
        for (const auto& item: videowall->items()->getItems())
            usedScreens.insert(item.uuid);
    }

    int count = VideoWallLicenseUsageHelper::licensesForScreens(usedScreens.size());
    int counter = 0;
    for (const auto& key: usedScreens)
    {
        if (counter < count)
            basicUsedLicenses[Qn::LC_VideoWall].push_back(key);
        if (counter < count + m_proposed)
            proposedToUse[Qn::LC_VideoWall].push_back(key);
        else
            break;
        ++counter;
    }
}

void VideoWallLicenseUsageHelper::propose(int count)
{
    m_proposed += count;
    invalidate();
}

int VideoWallLicenseUsageHelper::licensesForScreens(int screens)
{
    return (screens + 1) / 2;
}

/************************************************************************/
/* VideoWallLicenseUsageProposer                                      */
/************************************************************************/
VideoWallLicenseUsageProposer::VideoWallLicenseUsageProposer(
    common::SystemContext* context,
    VideoWallLicenseUsageHelper* helper,
    int screenCount)
    :
    m_helper(helper),
    m_count(0)
{
    if (!m_helper)
        return;

    int totalScreens = 0;
    const auto& resPool = context->resourcePool();

    // Calculate total screens used.
    for (const auto& videowall: resPool->getResources<QnVideoWallResource>())
        totalScreens += videowall->items()->getItems().size();

    const int totalLicensesUsed = VideoWallLicenseUsageHelper::licensesForScreens(totalScreens);

    // Proposed change for screens.
    const int proposedLicensesUsage = VideoWallLicenseUsageHelper::licensesForScreens(
        totalScreens + screenCount);

    m_count = proposedLicensesUsage - totalLicensesUsed;
    m_helper->propose(m_count);
}

VideoWallLicenseUsageProposer::~VideoWallLicenseUsageProposer()
{
    if (!m_helper)
        return;
    m_helper->propose(-m_count);
}

// --------------------------------------------------- ChannelPartnerServer ----------------------------------------------

const QString ChannelPartnerServer::baseUrl(common::SystemContext* context)
{
    auto result = context->globalSettings()->channelPartnerServerUrl().toString();
    result = result.trimmed();
    while (result.endsWith('/'))
        result.chop(1);
    return result;
}

const nx::utils::Url ChannelPartnerServer::reportUrl(common::SystemContext* context)
{
    const auto systemId = context->globalSettings()->cloudSystemId();
    return baseUrl(context) + NX_FMT("/api/v2/partners/cloud_systems/%1/system_usage_report/", systemId);
}

const nx::utils::Url ChannelPartnerServer::saasServicesUrl(common::SystemContext* context)
{
    const auto systemId = context->globalSettings()->cloudSystemId();
    return baseUrl(context) + NX_FMT("/api/v2/partners/cloud_systems/%1/services/", systemId);
}

const nx::utils::Url ChannelPartnerServer::saasDataUrl(common::SystemContext* context)
{
    const auto systemId = context->globalSettings()->cloudSystemId();
    return baseUrl(context) + NX_FMT("/api/v2/partners/cloud_systems/%1/saas_report/", systemId);
}

} // namespace nx::vms::license
