#include "license_usage_helper.h"

#include <numeric>
#include <functional>

#include <QtCore/QJsonObject>

#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <api/runtime_info_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>

#include <common/common_module.h>

#include <licensing/license_validator.h>


//#define QN_NO_LICENSE_CHECK

namespace {

QString joinedString(std::function<QString(Qn::LicenseType)> method, const QList<Qn::LicenseType> &licenseTypes)
{
    QStringList result;
    for (Qn::LicenseType lt : licenseTypes)
    {
        QString part = method(lt);
        if (!part.isEmpty())
            result << part;
    }
    return result.join(L'\n');
}

}

/** Allow to use 'master' license type instead of 'child' type if there are not enough child licenses. */
struct LicenseCompatibility
{
    LicenseCompatibility(Qn::LicenseType master, Qn::LicenseType child): master(master), child(child) {}
    Qn::LicenseType master;
    Qn::LicenseType child;
};

/* Compatibility tree:
 * Trial -> Edge -> Professional -> Analog -> (VMAX, AnalogEncoder)
 * Trial -> Edge -> Professional -> Bridge
 * Trial -> IO
 * Start -> Professional -> Analog -> (VMAX, AnalogEncoder)
 * Start -> Professional -> Bridge
 */
static std::array<LicenseCompatibility, 23> compatibleLicenseType =
{
    LicenseCompatibility(Qn::LC_Analog,         Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Analog,         Qn::LC_AnalogEncoder),

    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_Bridge),

    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_Bridge),

    LicenseCompatibility(Qn::LC_Start,          Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Start,          Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Start,          Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Start,          Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Start,          Qn::LC_Bridge),

    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Edge),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_IO),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Bridge)
};

/************************************************************************/
/* QnLicenseUsageHelper                                                 */
/************************************************************************/
QnLicenseUsageWatcher::QnLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(commonModule)
{
    connect(licensePool(), &QnLicensePool::licensesChanged, this,
        &QnLicenseUsageWatcher::licenseUsageChanged);

    // Call update if server was added or removed or changed its status.
    auto updateIfServerStatusChanged =
        [this](const QnPeerRuntimeInfo& data)
        {
            if (data.data.peer.peerType == Qn::PT_Server)
                emit licenseUsageChanged();
        };

    // Ignoring runtimeInfoChanged as hardwareIds must not change in runtime.
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, this,
        updateIfServerStatusChanged);
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, this,
        updateIfServerStatusChanged);
}


/************************************************************************/
/* QnLicenseUsageHelper                                                 */
/************************************************************************/

QnLicenseUsageHelper::Cache::Cache()
{
    boost::fill(used, 0);
    boost::fill(proposed, 0);
    boost::fill(overflow, 0);
    boost::fill(total, 0);
}


QnLicenseUsageHelper::QnLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(commonModule),
    m_dirty(true),
    m_validator(licensePool()->validator())
{
}

int QnLicenseUsageHelper::borrowLicenses(const LicenseCompatibility &compat, licensesArray &licenses) const
{
    auto borrowLicenseFromClass =
        [](int& srcUsed, int srcTotal, int& dstUsed, int dstTotal)
        {
            int borrowed = 0;
            if (dstUsed > dstTotal)
            {
                int donatorRest = srcTotal - srcUsed;
                if (donatorRest > 0)
                {
                    borrowed = qMin(donatorRest, dstUsed - dstTotal);
                    srcUsed += borrowed;
                    dstUsed -= borrowed;
                }
            }
            return borrowed;
        };

    return borrowLicenseFromClass(
        licenses[compat.master], m_cache.total[compat.master],
        licenses[compat.child], m_cache.total[compat.child]);
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseType licenseType) const
{
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 are used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getUsageMsg() const
{
    return joinedString([this](Qn::LicenseType lt){ return getUsageText(lt); }, licenseTypes());
}

QString QnLicenseUsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const
{
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 will be used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getProposedUsageMsg() const
{
    return joinedString([this](Qn::LicenseType lt){ return getProposedUsageText(lt); }, licenseTypes());
}

QString QnLicenseUsageHelper::getRequiredText(Qn::LicenseType licenseType) const
{
    if (requiredLicenses(licenseType) > 0)
        return tr("Activate %n more %1.", "", requiredLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    if (isValid() && proposedLicenses(licenseType) > 0)
        return tr("%n more %1 will be used.", "", proposedLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    return QString();
}

QString QnLicenseUsageHelper::getRequiredMsg() const
{
    return joinedString([this](Qn::LicenseType lt) { return getRequiredText(lt); }, licenseTypes());
}

void QnLicenseUsageHelper::invalidate()
{
    m_dirty = true;
}

void QnLicenseUsageHelper::setCustomValidator(QnLicenseValidator* validator)
{
    m_validator = validator;
}

void QnLicenseUsageHelper::updateCache() const
{
    if (!m_dirty)
        return;
    /* Need to set flag right here as virtual methods may call various cache-dependent getters. */
    m_dirty = false;

    m_cache.licenses.update(licensePool()->getLicenses());

    /* How many licenses of each type are borrowed. */
    licensesArray borrowedLicenses;
    boost::fill(borrowedLicenses, 0);

    /* Used licenses without proposed cameras. */
    licensesArray basicUsedLicenses;


    /* Borrowed licenses count without proposed cameras. */
    licensesArray basicBorrowedLicenses;
    boost::fill(basicBorrowedLicenses, 0);

    /* Calculate total licenses. */
    for (Qn::LicenseType lt : licenseTypes())
        m_cache.total[lt] = m_cache.licenses.totalLicenseByType(lt, m_validator.data());

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
        m_cache.overflow[lt] = calculateOverflowLicenses(lt, borrowedLicenses[lt]);
        m_cache.proposed[lt] = m_cache.used[lt] - basicUsedLicenses[lt];
    }
}

int QnLicenseUsageHelper::calculateOverflowLicenses(Qn::LicenseType licenseType, int borrowedLicenses) const
{
    Q_UNUSED(borrowedLicenses);
    return qMax(0, m_cache.used[licenseType] - m_cache.total[licenseType]);
}

bool QnLicenseUsageHelper::isValid() const
{
    return boost::algorithm::all_of(licenseTypes(), [this](Qn::LicenseType lt) { return isValid(lt); });
}

bool QnLicenseUsageHelper::isValid(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return true;
#endif

    updateCache();
    return m_cache.overflow[licenseType] == 0;
}

int QnLicenseUsageHelper::totalLicenses(Qn::LicenseType licenseType) const
{
    updateCache();
    return m_cache.total[licenseType];
}

int QnLicenseUsageHelper::usedLicenses(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return true;
#endif

    updateCache();
    /* In all cases but analog encoder licenses m_cache.used contains already valid number. */
    if (m_cache.overflow[licenseType] == 0)
        return m_cache.used[licenseType];
    return m_cache.total[licenseType] + m_cache.overflow[licenseType];
}

int QnLicenseUsageHelper::requiredLicenses(Qn::LicenseType licenseType) const
{
#ifdef QN_NO_LICENSE_CHECK
    return true;
#endif

    updateCache();
    return m_cache.overflow[licenseType];
}

int QnLicenseUsageHelper::proposedLicenses(Qn::LicenseType licenseType) const
{
    updateCache();
    return m_cache.proposed[licenseType];
}

QList<Qn::LicenseType> QnLicenseUsageHelper::licenseTypes() const
{
    if (m_licenseTypes.isEmpty())
        m_licenseTypes = calculateLicenseTypes();
    return m_licenseTypes;
}



/************************************************************************/
/* QnCamLicenseUsageWatcher                                             */
/************************************************************************/

QnCamLicenseUsageWatcher::QnCamLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent):
    QnCamLicenseUsageWatcher(QnVirtualCameraResourcePtr(), commonModule, parent)
{
}

QnCamLicenseUsageWatcher::QnCamLicenseUsageWatcher(
    const QnVirtualCameraResourcePtr& camera,
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(commonModule, parent)
{
    const auto& resPool = commonModule->resourcePool();

    /* Listening to all changes that can affect licenses usage. */
    auto connectToCamera =
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            connect(camera, &QnVirtualCameraResource::scheduleDisabledChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(camera, &QnVirtualCameraResource::licenseUsedChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(camera, &QnVirtualCameraResource::groupNameChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(camera, &QnVirtualCameraResource::groupIdChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(camera, &QnSecurityCamResource::licenseTypeChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
        };

    if (camera)
    {
        connectToCamera(camera);
        return;
    }

    /* Call update if camera was added or removed. */
    auto updateIfNeeded =
        [this](const QnResourcePtr &resource)
        {
            if (resource.dynamicCast<QnVirtualCameraResource>())
                emit licenseUsageChanged();
        };

    connect(resPool, &QnResourcePool::resourceAdded, this, updateIfNeeded);
    connect(resPool, &QnResourcePool::resourceRemoved, this, updateIfNeeded);

    connect(resPool, &QnResourcePool::resourceAdded, this,
        [this, connectToCamera](const QnResourcePtr& resource)
        {
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                connectToCamera(camera);
        });

    connect(resPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                camera->disconnect(this);
        });

    for (const auto& camera: resPool->getAllCameras(QnResourcePtr(), true))
        connectToCamera(camera);
}


/************************************************************************/
/* QnCamLicenseUsageHelper                                              */
/************************************************************************/
QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent):
    base_type(commonModule, parent),
    m_watcher(new QnCamLicenseUsageWatcher(commonModule, this))
{
    connect(m_watcher, &QnCamLicenseUsageWatcher::licenseUsageChanged, this,
        [this]()
        {
            invalidate();
            emit licenseUsageChanged();
        });
}

QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(
    const QnVirtualCameraResourceList& proposedCameras,
    bool proposedEnable,
    QnCommonModule* commonModule,
    QObject* parent)
    :
    QnCamLicenseUsageHelper(commonModule, parent)
{
    propose(proposedCameras, proposedEnable);
}

QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(
    const QnVirtualCameraResourcePtr& proposedCamera,
    bool proposedEnable,
    QnCommonModule* commonModule,
    QObject* parent)
:
    QnCamLicenseUsageHelper(commonModule, parent)
{
    propose(proposedCamera, proposedEnable);
}

void QnCamLicenseUsageHelper::propose(const QnVirtualCameraResourcePtr& proposedCamera, bool proposedEnable)
{
    return propose(QnVirtualCameraResourceList() << proposedCamera, proposedEnable);
}

void QnCamLicenseUsageHelper::propose(const QnVirtualCameraResourceList& proposedCameras, bool proposedEnable)
{
    if (proposedEnable)
    {
        m_proposedToEnable.unite(proposedCameras.toSet());
        m_proposedToDisable.intersect(proposedCameras.toSet());
    }
    else
    {
        m_proposedToDisable.unite(proposedCameras.toSet());
        m_proposedToEnable.intersect(proposedCameras.toSet());
    }
    invalidate();
}

bool QnCamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr& camera)
{
    return isOverflowForCamera(camera, camera->isLicenseUsed());
}

bool QnCamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr& camera,
    bool cachedLicenseUsed)
{
    bool requiresLicense = cachedLicenseUsed;
    requiresLicense &= !m_proposedToDisable.contains(camera);
    requiresLicense |= m_proposedToEnable.contains(camera);
    return requiresLicense && !isValid(camera->licenseType());
}


QList<Qn::LicenseType> QnCamLicenseUsageHelper::calculateLicenseTypes() const
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
        ;
}

void QnCamLicenseUsageHelper::calculateUsedLicenses(
    licensesArray& basicUsedLicenses,
    licensesArray& proposedToUse) const
{
    boost::fill(basicUsedLicenses, 0);
    boost::fill(proposedToUse, 0);
    const auto& resPool = commonModule()->resourcePool();

    auto groupId = [](const QnSecurityCamResourcePtr& camera)
    {
        return camera->isSharingLicenseInGroup()
            ? camera->getGroupId()
            : camera->getId().toString();
    };

    QMap<QString, QSet<QnSecurityCamResourcePtr>> oldCameras;
    for (const auto& camera: resPool->getAllCameras(QnResourcePtr(), true))
    {
        if (camera->isLicenseUsed())
            oldCameras[groupId(camera)].insert(camera);
    }

    for (const auto& data : oldCameras)
        basicUsedLicenses[(*data.begin())->licenseType()]++;

    auto newCameras = oldCameras;
    for (const auto& camera: m_proposedToEnable)
        newCameras[groupId(camera)].insert(camera);
    for (const auto& camera: m_proposedToDisable)
        newCameras[groupId(camera)].remove(camera);

    for (const auto& data: newCameras)
    {
        if (!data.isEmpty())
            proposedToUse[(*data.begin())->licenseType()]++;
    }

}

//////////////////////////////////////////////////////////////////////////

QnSingleCamLicenseStatusHelper::QnSingleCamLicenseStatusHelper(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    base_type(parent),
    m_camera(camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return;

    m_helper.reset(new QnCamLicenseUsageHelper(camera, true, camera->commonModule()));
    connect(m_helper, &QnCamLicenseUsageHelper::licenseUsageChanged,
        this, &QnSingleCamLicenseStatusHelper::licenseStatusChanged);
}

QnSingleCamLicenseStatusHelper::~QnSingleCamLicenseStatusHelper()
{
    if (!m_camera)
        return;

    m_camera->disconnect(this);
    m_helper->disconnect(this);
}

QnLicenseUsageStatus QnSingleCamLicenseStatusHelper::status() const
{
    if (!m_camera)
        return QnLicenseUsageStatus::invalid;

    const bool isLicenseUsed = m_camera->isLicenseUsed();
    if (m_helper->isOverflowForCamera(m_camera, isLicenseUsed))
        return QnLicenseUsageStatus::overflow;

    return (isLicenseUsed ? QnLicenseUsageStatus::used : QnLicenseUsageStatus::notUsed);
}


/************************************************************************/
/* QnVideoWallLicenseUsageWatcher                                       */
/************************************************************************/
QnVideoWallLicenseUsageWatcher::QnVideoWallLicenseUsageWatcher(
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(commonModule, parent)
{
    auto updateIfNeeded =
        [this](const QnResourcePtr& resource)
        {
            if (!resource.dynamicCast<QnVideoWallResource>())
                return;
            emit licenseUsageChanged();
        };

    auto connectTo =
        [this](const QnVideoWallResourcePtr& videowall)
        {
            connect(videowall, &QnVideoWallResource::itemAdded, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(videowall, &QnVideoWallResource::itemChanged, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
            connect(videowall, &QnVideoWallResource::itemRemoved, this,
                &QnLicenseUsageWatcher::licenseUsageChanged);
        };

    auto connectIfNeeded =
        [this, connectTo](const QnResourcePtr& resource)
        {
            if (const auto& videowall = resource.dynamicCast<QnVideoWallResource>())
                connectTo(videowall);
        };

    const auto& resPool = commonModule->resourcePool();

    connect(resPool, &QnResourcePool::resourceAdded, this, connectIfNeeded);
    connect(resPool, &QnResourcePool::resourceAdded, this, updateIfNeeded);
    connect(resPool, &QnResourcePool::resourceRemoved, this, updateIfNeeded);
    for (const auto& videowall: resPool->getResources<QnVideoWallResource>())
        connectTo(videowall);
}


/************************************************************************/
/* QnVideoWallLicenseUsageHelper                                        */
/************************************************************************/
QnVideoWallLicenseUsageHelper::QnVideoWallLicenseUsageHelper(
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(commonModule, parent)
{
    auto usageWatcher = new QnVideoWallLicenseUsageWatcher(commonModule, this);
    connect(usageWatcher, &QnVideoWallLicenseUsageWatcher::licenseUsageChanged, this,
        &QnLicenseUsageHelper::invalidate);
}

QList<Qn::LicenseType> QnVideoWallLicenseUsageHelper::calculateLicenseTypes() const
{
    return QList<Qn::LicenseType>() << Qn::LC_VideoWall;
}

void QnVideoWallLicenseUsageHelper::calculateUsedLicenses(licensesArray& basicUsedLicenses,
    licensesArray& proposedToUse) const
{
    boost::fill(basicUsedLicenses, 0);
    boost::fill(proposedToUse, 0);

    int usedScreens = 0;
    int controlSessions = 0;
    const auto& resPool = commonModule()->resourcePool();
    for (const QnVideoWallResourcePtr &videowall : resPool->getResources<QnVideoWallResource>())
    {
        /* Calculating total screens. */
        usedScreens += videowall->items()->getItems().size();

        /* Calculating running control sessions. */
        for (const QnVideoWallItem &item : videowall->items()->getItems())
            if (!item.runtimeStatus.controlledBy.isNull())
                ++controlSessions;
    }

    basicUsedLicenses[Qn::LC_VideoWall] = qMax(controlSessions,
        QnVideoWallLicenseUsageHelper::licensesForScreens(usedScreens));
    proposedToUse[Qn::LC_VideoWall] = basicUsedLicenses[Qn::LC_VideoWall] + m_proposed;
}

void QnVideoWallLicenseUsageHelper::propose(int count)
{
    m_proposed += count;
    invalidate();
}

int QnVideoWallLicenseUsageHelper::licensesForScreens(int screens)
{
    return (screens + 1) / 2;
}


/************************************************************************/
/* QnVideoWallLicenseUsageProposer                                      */
/************************************************************************/
QnVideoWallLicenseUsageProposer::QnVideoWallLicenseUsageProposer(
    QnVideoWallLicenseUsageHelper* helper,
    int screenCount,
    int controlSessionsCount)
    :
    m_helper(helper),
    m_count(0)
{
    if (!m_helper)
        return;

    int totalScreens = 0;
    int controlSessions = 0;
    const auto& resPool = helper->commonModule()->resourcePool();
    for (const auto& videowall: resPool->getResources<QnVideoWallResource>())
    {
        /* Calculate total screens used. */
        totalScreens += videowall->items()->getItems().size();

        /* Calculating running control sessions. */
        for (const QnVideoWallItem &item : videowall->items()->getItems())
            if (!item.runtimeStatus.controlledBy.isNull())
                ++controlSessions;
    }
    int screensLicensesUsed = QnVideoWallLicenseUsageHelper::licensesForScreens(totalScreens);

    /* Select which requirement is currently in action. */
    int totalLicensesUsed = qMax(screensLicensesUsed, controlSessions);

    /* Proposed change for screens. */
    int screensValue = QnVideoWallLicenseUsageHelper::licensesForScreens(totalScreens + screenCount);

    /* Proposed change for control sessions. */
    int controlSessionsValue = controlSessions + controlSessionsCount;

    /* Select proposed requirement. */
    int proposedLicensesUsage = qMax(controlSessionsValue, screensValue);

    m_count = proposedLicensesUsage - totalLicensesUsed;
    m_helper->propose(m_count);
}

QnVideoWallLicenseUsageProposer::~QnVideoWallLicenseUsageProposer()
{
    if (!m_helper)
        return;
    m_helper->propose(-m_count);
}
