#include "license_usage_helper.h"

#include <api/runtime_info_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>
#include "qjsonobject.h"
#include "mustache/mustache.h"


/** Allow to use 'master' license type instead of 'child' type if there are not enough child licenses. */
struct LicenseCompatibility 
{
    LicenseCompatibility(Qn::LicenseType master, Qn::LicenseType child): master(master), child(child) {}
    Qn::LicenseType master;
    Qn::LicenseType child;
};

static std::array<LicenseCompatibility, 14> compatibleLicenseType =
{
    LicenseCompatibility(Qn::LC_Edge,    Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Professional, Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Edge,    Qn::LC_Analog),

    LicenseCompatibility(Qn::LC_Analog,    Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Professional, Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Edge, Qn::LC_VMAX),

    LicenseCompatibility(Qn::LC_Analog,    Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Professional, Qn::LC_AnalogEncoder),
    LicenseCompatibility(Qn::LC_Edge, Qn::LC_AnalogEncoder),

    LicenseCompatibility(Qn::LC_Trial,    Qn::LC_Edge),
    LicenseCompatibility(Qn::LC_Trial,    Qn::LC_Professional),
    LicenseCompatibility(Qn::LC_Trial,    Qn::LC_Analog),
    LicenseCompatibility(Qn::LC_Trial,    Qn::LC_VMAX),
    LicenseCompatibility(Qn::LC_Trial,    Qn::LC_AnalogEncoder)
};

/************************************************************************/
/* QnLicenseUsageHelper                                                 */
/************************************************************************/

QnLicenseUsageHelper::QnLicenseUsageHelper(QObject *parent):
    base_type(parent),
    m_licenses(qnLicensePool->getLicenses())
{
    memset(m_usedLicenses, 0, sizeof(m_usedLicenses));
    memset(m_proposedLicenses, 0, sizeof(m_proposedLicenses));
    memset(m_overflowLicenses, 0, sizeof(m_overflowLicenses));

    connect(qnLicensePool, &QnLicensePool::licensesChanged, this, &QnLicenseUsageHelper::update);
}

void QnLicenseUsageHelper::borrowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal)
{
    if (dstUsed > dstTotal) {
        int donatorRest = srcTotal - srcUsed;
        if (donatorRest > 0) {
            int moveCnt = qMin(donatorRest, dstUsed - dstTotal);
            srcUsed += moveCnt;
            dstUsed -= moveCnt;
        }
    }
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseType licenseType) const {
    if (m_usedLicenses[licenseType] == 0)
        return QString();
    return tr("%n %2 are used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getUsageText() const 
{
    QString licenseText;
    foreach (Qn::LicenseType lt, licenseTypes()) {
//         if (totalLicense(lt) == 0)
//             continue;
        if (!licenseText.isEmpty())
            licenseText += lit("\n");
        licenseText += getUsageText(lt);
    }
    return licenseText;
}

QString QnLicenseUsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const {
    if (m_usedLicenses[licenseType] == 0)
        return QString();
    return tr("%n %2 will be used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getProposedUsageText() const
{
    QString licenseText;
    foreach (Qn::LicenseType lt, licenseTypes()) {
        if (!licenseText.isEmpty())
            licenseText += lit("\n");
        licenseText += getProposedUsageText(lt);

    }
    return licenseText;
}


QString QnLicenseUsageHelper::getRequiredLicenseMsg(Qn::LicenseType licenseType) const {
    if (!isValid() && m_overflowLicenses[licenseType] > 0) 
        return tr("Activate %n more %2. ", "", m_overflowLicenses[licenseType]).arg(QnLicense::longDisplayName(licenseType));
    if (isValid() && m_proposedLicenses[licenseType] > 0)        
        return tr("%n more %2 will be used. ", "", m_proposedLicenses[licenseType]).arg(QnLicense::longDisplayName(licenseType));
    return QString();
}

QString QnLicenseUsageHelper::getRequiredLicenseMsg() const
{
    QString licenseText;
    foreach (Qn::LicenseType lt, licenseTypes()) {
        if (!licenseText.isEmpty())
            licenseText += lit("\n");
        licenseText += getRequiredLicenseMsg(lt);

    }
    return licenseText;
}

void QnLicenseUsageHelper::update() {
    m_licenses.update(qnLicensePool->getLicenses());

    int maxTotal = 0;
    int maxLicenses[Qn::LC_Count];
    foreach (Qn::LicenseType lt, licenseTypes()) {
        m_usedLicenses[lt] = calculateUsedLicenses(lt) + m_proposedLicenses[lt];
        maxLicenses[lt] = m_licenses.totalLicenseByType(lt);
        maxTotal += maxLicenses[lt];
    }

    foreach (Qn::LicenseType lt, licenseTypes()) {
        foreach(const LicenseCompatibility& c, compatibleLicenseType) {
            if (c.child == lt)
                borrowLicenseFromClass(m_usedLicenses[c.master], maxLicenses[c.master], m_usedLicenses[lt], maxLicenses[lt]);
        }
    }

    foreach (Qn::LicenseType lt, licenseTypes())
        m_overflowLicenses[lt] = qMax(0, m_usedLicenses[lt] - maxLicenses[lt]);

    emit licensesChanged();
}

bool QnLicenseUsageHelper::isValid() const {
    foreach (Qn::LicenseType lt, licenseTypes())
        if (!isValid(lt))
            return false;
    return true;
}

bool QnLicenseUsageHelper::isValid(Qn::LicenseType licenseType) const {
    return m_overflowLicenses[licenseType] == 0;
}

int QnLicenseUsageHelper::totalLicense(Qn::LicenseType licenseType) const {
    return m_licenses.totalLicenseByType(licenseType);
}

int QnLicenseUsageHelper::usedLicense(Qn::LicenseType licenseType) const {
    return m_usedLicenses[licenseType];
}

QList<Qn::LicenseType> QnLicenseUsageHelper::licenseTypes() const {
    if (m_licenseTypes.isEmpty())
        m_licenseTypes = calculateLicenseTypes();
    return m_licenseTypes;
}

/************************************************************************/
/* QnCamLicenseUsageHelper                                              */
/************************************************************************/
QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(QObject *parent):
    QnLicenseUsageHelper(parent)
{
    init();
    update();
}

QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable, QObject *parent):
    QnLicenseUsageHelper(parent)
{
    init();
    // update will be called inside
    propose(proposedCameras, proposedEnable);
}

void QnCamLicenseUsageHelper::init() {
    auto updateIfNeeded = [this](const QnResourcePtr &resource) {
        if (!resource.dynamicCast<QnMediaServerResource>())
            return;
        update();
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   updateIfNeeded);
    connect(qnResPool, &QnResourcePool::statusChanged,   this,   updateIfNeeded);
}

void QnCamLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    foreach (const QnVirtualCameraResourcePtr &camera, proposedCameras) 
    {
        QnResourcePtr mserver = qnResPool->getResourceById(camera->getParentId());
        if (!mserver || mserver->getStatus() == Qn::Offline)
            continue;

        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) 
            m_proposedLicenses[camera->licenseType()]++;
    }
    if (!proposedEnable) {
        foreach (Qn::LicenseType lt, licenseTypes())
            m_proposedLicenses[lt] *= -1;
    }
    update();
}

bool QnCamLicenseUsageHelper::isOverflowForCamera(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled() && m_overflowLicenses[camera->licenseType()];
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

int QnCamLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType) const {
    return qnResPool->activeCamerasByLicenseType(licenseType);
}

/************************************************************************/
/* QnVideoWallLicenseUsageHelper                                        */
/************************************************************************/
QnVideoWallLicenseUsageHelper::QnVideoWallLicenseUsageHelper(QObject *parent):
    QnLicenseUsageHelper(parent)
{
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoAdded,    this, &QnVideoWallLicenseUsageHelper::update);
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoChanged,  this, &QnVideoWallLicenseUsageHelper::update);
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoRemoved,  this, &QnVideoWallLicenseUsageHelper::update);

    update();
}

QList<Qn::LicenseType> QnVideoWallLicenseUsageHelper::calculateLicenseTypes() const {
    return QList<Qn::LicenseType>() << Qn::LC_VideoWall;
}

int QnVideoWallLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType) const {
    Q_ASSERT(licenseType == Qn::LC_VideoWall);
    int result = 0;
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems())
        /* Calculating running control sessions. */
        result += info.data.videoWallControlSessions;

    return result;
}

void QnVideoWallLicenseUsageHelper::propose(const QnVideoWallResourcePtr &videowall, const QUuid &pcUuid, int itemsCount) {
    int used = 0;
    if (!pcUuid.isNull()) {
        foreach(const QnVideoWallItem &item, videowall->items()->getItems()) {
            if (item.pcUuid != pcUuid)
                continue;
            used++;
        }
    }
    m_proposedLicenses[Qn::LC_VideoWall] = itemsCount - used;
    update();
}

void QnVideoWallLicenseUsageHelper::propose(int count) {
    m_proposedLicenses[Qn::LC_VideoWall] += count;
    update();
}

QnVideoWallLicenseUsageProposer::QnVideoWallLicenseUsageProposer(QnVideoWallLicenseUsageHelper* helper, int count):
    m_helper(helper),
    m_count(count)
{
    if (m_helper)
        m_helper->propose(m_count);
}

QnVideoWallLicenseUsageProposer::~QnVideoWallLicenseUsageProposer() {
    if (m_helper)
        m_helper->propose(-m_count);
}

QString QnLicenseUsageHelper::activationMessage(const QJsonObject& errorMessage)
{
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