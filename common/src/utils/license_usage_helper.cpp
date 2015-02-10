#include "license_usage_helper.h"

#ifdef ENABLE_SENDMAIL

#include <numeric>
#include <functional>

#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>
#include "qjsonobject.h"
#include "mustache/mustache.h"

namespace {

    QString joinedString(std::function<QString(Qn::LicenseType)> method, const QList<Qn::LicenseType> &licenseTypes) {
        QStringList result;
        for(Qn::LicenseType lt: licenseTypes) {
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

/* Compatibility tree: Trial -> Edge -> Professional -> Analog -> (VMAX, AnalogEncoder) */
static std::array<LicenseCompatibility, 14> compatibleLicenseType =
{
    LicenseCompatibility(Qn::LC_Analog,         Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Analog,         Qn::LC_AnalogEncoder),

    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Professional,   Qn::LC_AnalogEncoder),

    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Edge,           Qn::LC_AnalogEncoder),

    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Edge),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Trial,          Qn::LC_AnalogEncoder)
};

/************************************************************************/
/* QnLicenseUsageHelper                                                 */
/************************************************************************/
QnLicenseUsageWatcher::QnLicenseUsageWatcher(QObject* parent /*= NULL*/):
    base_type(parent)
{

    /* Call update if server was added or removed or changed its status. */
    auto updateIfNeeded = [this](const QnResourcePtr &resource) {
        if (resource.dynamicCast<QnMediaServerResource>())
            emit licenseUsageChanged();
    };

    connect(qnResPool,      &QnResourcePool::resourceAdded,     this,   updateIfNeeded);
    connect(qnResPool,      &QnResourcePool::statusChanged,     this,   updateIfNeeded);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,   this,   updateIfNeeded);

    connect(qnLicensePool,  &QnLicensePool::licensesChanged,    this,   &QnLicenseUsageWatcher::licenseUsageChanged);
}


/************************************************************************/
/* QnLicenseUsageHelper                                                 */
/************************************************************************/

QnLicenseUsageHelper::Cache::Cache() {
    boost::fill(used, 0);
    boost::fill(proposed, 0);
    boost::fill(overflow, 0);
    boost::fill(total, 0);
}


QnLicenseUsageHelper::QnLicenseUsageHelper(QObject *parent):
    base_type(parent),
    m_dirty(true)
{
}

int QnLicenseUsageHelper::borrowLicenses(const LicenseCompatibility &compat, licensesArray &licenses) const {

    auto borrowLicenseFromClass = [] (int& srcUsed, int srcTotal, int& dstUsed, int dstTotal) {
        int borrowed = 0;
        if (dstUsed > dstTotal) { 
            int donatorRest = srcTotal - srcUsed;
            if (donatorRest > 0) {
                borrowed = qMin(donatorRest, dstUsed - dstTotal);
                srcUsed += borrowed;
                dstUsed -= borrowed;
            }
        }
        return borrowed;
    };

    return borrowLicenseFromClass(licenses[compat.master], m_cache.total[compat.master], licenses[compat.child], m_cache.total[compat.child]);
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseType licenseType) const {
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 are used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getUsageMsg() const {
    return joinedString(std::bind(&QnLicenseUsageHelper::getUsageText, this, std::placeholders::_1), licenseTypes());
}

QString QnLicenseUsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const {
    if (usedLicenses(licenseType) == 0)
        return QString();
    return tr("%n %2 will be used out of %1.", "", usedLicenses(licenseType)).arg(totalLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getProposedUsageMsg() const {
    return joinedString(std::bind(&QnLicenseUsageHelper::getProposedUsageText, this, std::placeholders::_1), licenseTypes());
}

QString QnLicenseUsageHelper::getRequiredText(Qn::LicenseType licenseType) const {
    if (requiredLicenses(licenseType) > 0) 
        return tr("Activate %n more %2. ", "", requiredLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    if (isValid() && proposedLicenses(licenseType) > 0)        
        return tr("%n more %2 will be used. ", "", proposedLicenses(licenseType)).arg(QnLicense::longDisplayName(licenseType));

    return QString();
}

QString QnLicenseUsageHelper::getRequiredMsg() const {
    return joinedString(std::bind(&QnLicenseUsageHelper::getRequiredText, this, std::placeholders::_1), licenseTypes());
}

void QnLicenseUsageHelper::invalidate() {
    m_dirty = true;
}

void QnLicenseUsageHelper::updateCache() const {
    if (!m_dirty)
        return;
    /* Need to set flag right here as virtual methods may call various cache-dependent getters. */
    m_dirty = false;

    m_cache.licenses.update(qnLicensePool->getLicenses());

    /* How many licenses of each type are borrowed. */
    licensesArray borrowedLicenses;
    boost::fill(borrowedLicenses, 0);

    /* Used licenses without proposed cameras. */
    licensesArray basicUsedLicenses;
    boost::fill(basicUsedLicenses, 0);

    /* Borrowed licenses count without proposed cameras. */
    licensesArray basicBorrowedLicenses;
    boost::fill(basicBorrowedLicenses, 0);

    /* Calculate total licenses. */
    for (Qn::LicenseType lt: licenseTypes())
        m_cache.total[lt] = m_cache.licenses.totalLicenseByType(lt);

    /* Calculate used licenses with and without proposed cameras (to get proposed value as difference). */
    for (Qn::LicenseType lt: licenseTypes()) {
        basicUsedLicenses[lt] = calculateUsedLicenses(lt, false);
        m_cache.used[lt] = calculateUsedLicenses(lt, true);
    }

    /* Borrow some licenses (if available). Also repeating with and without proposed cameras. */
    for(const LicenseCompatibility& c: compatibleLicenseType) {
        basicBorrowedLicenses[c.child] += borrowLicenses(c, basicUsedLicenses);
        borrowedLicenses[c.child]      += borrowLicenses(c, m_cache.used);
    }

    /* Finally calculating proposed and lacking (overflow) licenses. */
    for (Qn::LicenseType lt: licenseTypes()) {
        m_cache.overflow[lt] = calculateOverflowLicenses(lt, borrowedLicenses[lt]);
        m_cache.proposed[lt] = m_cache.used[lt] - basicUsedLicenses[lt];
    }
}

int QnLicenseUsageHelper::calculateOverflowLicenses(Qn::LicenseType licenseType, int borrowedLicenses) const {
    Q_UNUSED(borrowedLicenses);
    return qMax(0, m_cache.used[licenseType] - m_cache.total[licenseType]);
}

bool QnLicenseUsageHelper::isValid() const {
    return boost::algorithm::all_of(licenseTypes(), [this](Qn::LicenseType lt){ return isValid(lt); });
}

bool QnLicenseUsageHelper::isValid(Qn::LicenseType licenseType) const {
    updateCache();
    return m_cache.overflow[licenseType] == 0;
}

int QnLicenseUsageHelper::totalLicenses(Qn::LicenseType licenseType) const {
    updateCache();
    return m_cache.total[licenseType];
}

int QnLicenseUsageHelper::usedLicenses(Qn::LicenseType licenseType) const {
    updateCache();
    /* In all cases but analog encoder licenses m_cache.used contains already valid number. */
    if (m_cache.overflow[licenseType] == 0)
        return m_cache.used[licenseType];
    return m_cache.total[licenseType] + m_cache.overflow[licenseType];
}

int QnLicenseUsageHelper::requiredLicenses(Qn::LicenseType licenseType) const {
    updateCache();
    return m_cache.overflow[licenseType];
}

int QnLicenseUsageHelper::proposedLicenses(Qn::LicenseType licenseType) const {
    updateCache();
    return m_cache.proposed[licenseType];
}

QList<Qn::LicenseType> QnLicenseUsageHelper::licenseTypes() const {
    if (m_licenseTypes.isEmpty())
        m_licenseTypes = calculateLicenseTypes();
    return m_licenseTypes;
}

QString QnLicenseUsageHelper::activationMessage(const QJsonObject& errorMessage) {
    QString messageId = errorMessage.value(lit("messageId")).toString();
    QString message = errorMessage.value(lit("message")).toString();
    QVariantMap arguments = errorMessage.value(lit("arguments")).toObject().toVariantMap();

    if(messageId == lit("DatabaseError")) {
        message = tr("There was a problem activating your license key. Database error has occurred.");  //TODO: Feature #3629 case J
    } else if(messageId == lit("InvalidData")) {
        message = tr("There was a problem activating your license key. Invalid data received. Please contact support team to report issue.");
    } else if(messageId == lit("InvalidKey")) {
        message = tr("The license key you have entered is invalid. Please check that license key is entered correctly. "
            "If problem continues, please contact support team to confirm if license key is valid or to get a valid license key.");
    } else if(messageId == lit("InvalidBrand")) {
        message = tr("You are trying to activate an incompatible license with your software. Please contact support team to get a valid license key.");
    } else if(messageId == lit("AlreadyActivated")) {
        message = tr("This license key has been previously activated to hardware id {{hwid}} on {{time}}. Please contact support team to get a valid license key.");
    }

    return Mustache::renderTemplate(message, arguments);
}

/************************************************************************/
/* QnCamLicenseUsageWatcher                                             */
/************************************************************************/

QnCamLicenseUsageWatcher::QnCamLicenseUsageWatcher(QObject* parent /*= NULL*/):
    base_type(parent)
{
    /* Call update if camera was added or removed. */
    auto updateIfNeeded = [this](const QnResourcePtr &resource) {
        if (resource.dynamicCast<QnVirtualCameraResource>())
            emit licenseUsageChanged();
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   updateIfNeeded);

    /* Listening to all changes that can affect licenses usage. */
    auto connectToCamera = [this](const QnVirtualCameraResourcePtr &camera) {
        connect(camera, &QnVirtualCameraResource::scheduleDisabledChanged,  this, &QnLicenseUsageWatcher::licenseUsageChanged);
        connect(camera, &QnVirtualCameraResource::groupNameChanged,         this, &QnLicenseUsageWatcher::licenseUsageChanged);
        connect(camera, &QnVirtualCameraResource::groupIdChanged,           this, &QnLicenseUsageWatcher::licenseUsageChanged);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   [this, connectToCamera](const QnResourcePtr &resource) {
        if (const QnVirtualCameraResourcePtr &camera = resource.dynamicCast<QnVirtualCameraResource>())
            connectToCamera(camera);
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   [this](const QnResourcePtr &resource) {
        if (const QnVirtualCameraResourcePtr &camera = resource.dynamicCast<QnVirtualCameraResource>())
            disconnect(camera, NULL, this, NULL);
    });

    for (const QnVirtualCameraResourcePtr &camera: qnResPool->getResources<QnVirtualCameraResource>())
        connectToCamera(camera);
}


/************************************************************************/
/* QnCamLicenseUsageHelper                                              */
/************************************************************************/
QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(QObject *parent):
    base_type(parent)
{
    init();
}

QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable, QObject *parent):
    base_type(parent)
{
    init();
    propose(proposedCameras, proposedEnable);
}

void QnCamLicenseUsageHelper::init() {
    QnCamLicenseUsageWatcher* usageWatcher = new QnCamLicenseUsageWatcher(this);
    connect(usageWatcher, &QnCamLicenseUsageWatcher::licenseUsageChanged, this, &QnLicenseUsageHelper::invalidate);
}

bool QnCamLicenseUsageHelper::cameraRequiresLicense(const QnVirtualCameraResourcePtr &camera, Qn::LicenseType licenseType, bool countProposed) const {
    if (camera->licenseType() != licenseType) 
        return false;

    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server || server->getStatus() != Qn::Online)
        return false;

    bool requiresLicense = !camera->isScheduleDisabled();
    if (countProposed) {
        requiresLicense &= !m_proposedToDisable.contains(camera);
        requiresLicense |= m_proposedToEnable.contains(camera);
    }
    return requiresLicense;
}

void QnCamLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    if (proposedEnable) {
        m_proposedToEnable.unite(proposedCameras.toSet());
        m_proposedToDisable.intersect(proposedCameras.toSet());
    } else {
        m_proposedToDisable.unite(proposedCameras.toSet());
        m_proposedToEnable.intersect(proposedCameras.toSet());
    }
    invalidate();
}

bool QnCamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled() && !isValid(camera->licenseType());
}

QList<Qn::LicenseType> QnCamLicenseUsageHelper::calculateLicenseTypes() const {
    return QList<Qn::LicenseType>()
        << Qn::LC_Trial
        << Qn::LC_Analog    // both places
        << Qn::LC_Professional
        << Qn::LC_Edge
        << Qn::LC_VMAX  //only main page
        << Qn::LC_AnalogEncoder
        ;
}

int QnCamLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType, bool countProposed) const {
    if (licenseType == Qn::LC_AnalogEncoder) {

        QList<int> cameraSets = analogEncoderCameraSets(countProposed);
        int total = totalLicenses(licenseType);

        /* If we have enough licenses for all the encoders, return max value. */
        int required = cameraSets.size();
        if (required <= total)
            return required;

        /* Use licenses for the maximum possible number of cameras. */ 
        return total + std::accumulate(cameraSets.cbegin() + total, cameraSets.cend(), 0);      
    } else {
        int count = 0;
        for (const QnVirtualCameraResourcePtr &camera: qnResPool->getResources<QnVirtualCameraResource>()) {
            if (cameraRequiresLicense(camera, licenseType, countProposed))
                count++;
        }
        return count;
    }
}

int QnCamLicenseUsageHelper::calculateOverflowLicenses(Qn::LicenseType licenseType, int borrowedLicenses) const {
    if (licenseType != Qn::LC_AnalogEncoder)
        return base_type::calculateOverflowLicenses(licenseType, borrowedLicenses);

    QList<int> cameraSets = analogEncoderCameraSets(true);
    int total = totalLicenses(licenseType);

    /* If we have enough licenses for all the encoders, return max value. */
    int required = cameraSets.size();
    if (required <= total)
        return 0;

    /* Count existing licenses. */
    for (int i = 0; i < total; ++i)
        cameraSets.removeFirst();
  
    /* Check how many licenses are borrowed. */
    int borrowed = borrowedLicenses;

    /* Use borrowed licenses for the maximum possible sets of cameras. */    
    while (!cameraSets.empty() && borrowed >= cameraSets.last())
        borrowed -= cameraSets.takeLast();

    return cameraSets.size();
}

QList<int> QnCamLicenseUsageHelper::analogEncoderCameraSets(bool countProposed) const {
    QHash<QString, int> m_camerasByGroupId;

    for (const QnVirtualCameraResourcePtr &camera: qnResPool->getResources<QnVirtualCameraResource>()) {
        if (cameraRequiresLicense(camera, Qn::LC_AnalogEncoder, countProposed))
            m_camerasByGroupId[camera->getGroupId()]++;
    }

    int limit = QnLicensePool::camerasPerAnalogEncoder();

    QList<int> cameraSets;

    /* Calculate how many encoders use full set of cameras. */
    for (int camerasInGroup: m_camerasByGroupId) {
        int fullSets = camerasInGroup / limit;
        int remainder = camerasInGroup % limit;

        for (int i = 0; i < fullSets; ++i)
            cameraSets << limit;

        /* Get all encoders that require license for non-full set of cameras. */
        if (remainder > 0)
            cameraSets << remainder;
    }

    boost::sort(cameraSets | boost::adaptors::reversed);

    return cameraSets;
}

/************************************************************************/
/* QnVideoWallLicenseUsageWatcher                                       */
/************************************************************************/
QnVideoWallLicenseUsageWatcher::QnVideoWallLicenseUsageWatcher(QObject* parent /*= NULL*/):
    base_type(parent)
{
    auto updateIfNeeded = [this](const QnResourcePtr &resource) {
        if (!resource.dynamicCast<QnVideoWallResource>())
            return;
        emit licenseUsageChanged();
    };

    auto connectTo = [this](const QnVideoWallResourcePtr &videowall) {
        connect(videowall, &QnVideoWallResource::itemAdded,     this, &QnLicenseUsageWatcher::licenseUsageChanged);
        connect(videowall, &QnVideoWallResource::itemChanged,   this, &QnLicenseUsageWatcher::licenseUsageChanged);
        connect(videowall, &QnVideoWallResource::itemRemoved,   this, &QnLicenseUsageWatcher::licenseUsageChanged);
    };

    auto connectIfNeeded = [this, connectTo](const QnResourcePtr &resource) {
        if (QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>())
            connectTo(videowall);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   connectIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   updateIfNeeded);
    for (const QnVideoWallResourcePtr &videowall: qnResPool->getResources<QnVideoWallResource>())
        connectTo(videowall);
}


/************************************************************************/
/* QnVideoWallLicenseUsageHelper                                        */
/************************************************************************/
QnVideoWallLicenseUsageHelper::QnVideoWallLicenseUsageHelper(QObject *parent):
    QnLicenseUsageHelper(parent),
    m_proposed(0)
{
   QnVideoWallLicenseUsageWatcher* usageWatcher = new QnVideoWallLicenseUsageWatcher(this);
   connect(usageWatcher, &QnVideoWallLicenseUsageWatcher::licenseUsageChanged, this, &QnLicenseUsageHelper::invalidate);
}

QList<Qn::LicenseType> QnVideoWallLicenseUsageHelper::calculateLicenseTypes() const {
    return QList<Qn::LicenseType>() << Qn::LC_VideoWall;
}

int QnVideoWallLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType, bool countProposed) const {
    Q_ASSERT(licenseType == Qn::LC_VideoWall);
   
    int usedScreens = 0;
    int controlSessions = 0;
    for (const QnVideoWallResourcePtr &videowall: qnResPool->getResources<QnVideoWallResource>()) {
        /* Calculating total screens. */
        usedScreens += videowall->items()->getItems().size();

        /* Calculating running control sessions. */
        for (const QnVideoWallItem &item: videowall->items()->getItems())
            if (!item.runtimeStatus.controlledBy.isNull())
                ++controlSessions;
    }

    int result = qMax(controlSessions, QnVideoWallLicenseUsageHelper::licensesForScreens(usedScreens));
    if (countProposed)
        return result + m_proposed;
    return result;
}

void QnVideoWallLicenseUsageHelper::propose(int count) {
    m_proposed += count;
    invalidate();
}

int QnVideoWallLicenseUsageHelper::licensesForScreens(int screens) {
    return (screens + 1) / 2;
}


/************************************************************************/
/* QnVideoWallLicenseUsageProposer                                      */
/************************************************************************/
QnVideoWallLicenseUsageProposer::QnVideoWallLicenseUsageProposer(QnVideoWallLicenseUsageHelper* helper, int screenCount, int controlSessionsCount):
    m_helper(helper),
    m_count(0)
{
    if (!m_helper)
        return;

    int totalScreens = 0;
    int controlSessions = 0;
    for (const QnVideoWallResourcePtr &videowall: qnResPool->getResources<QnVideoWallResource>()) {
        /* Calculate total screens used. */
        totalScreens += videowall->items()->getItems().size();

        /* Calculating running control sessions. */
        for (const QnVideoWallItem &item: videowall->items()->getItems())
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

QnVideoWallLicenseUsageProposer::~QnVideoWallLicenseUsageProposer() {
    if (!m_helper)
        return;
    m_helper->propose(-m_count);
}

#endif //ENABLE_SENDMAIL
