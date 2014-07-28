#include "license_usage_helper.h"

#include <api/runtime_info_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>


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
    m_licenses(qnLicensePool->getLicenses()),
    m_isValid(true)
{
    memset(m_usedLicenses, 0, sizeof(m_usedLicenses));
    memset(m_proposedLicenses, 0, sizeof(m_proposedLicenses));
    memset(m_overflowLicenses, 0, sizeof(m_overflowLicenses));
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

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseType licenseType) const
{
    return tr("%n %2 are used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getUsageText() const 
{
    QString licenseText;
    foreach (Qn::LicenseType lt, licenseTypes()) {
        if (totalLicense(lt) == 0)
            continue;
        if (!licenseText.isEmpty())
            licenseText += lit("\n");
        licenseText += getUsageText(lt);
    }
    return licenseText;
}

QString QnLicenseUsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const
{
    return tr("%n %2 will be used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getProposedUsageText() const
{
    QString licenseText;
    foreach (Qn::LicenseType lt, licenseTypes()) {
        if (lt != Qn::LC_Professional && totalLicense(lt) == 0)
            continue;
        if (!licenseText.isEmpty())
            licenseText += lit("\n");
        licenseText += getProposedUsageText(lt);

    }
    return licenseText;
}

QString QnLicenseUsageHelper::getRequiredLicenseMsg() const
{
    QString msg;

    if (!isValid()) 
    {
        foreach (Qn::LicenseType lt, licenseTypes()) {
            if (m_overflowLicenses[lt] > 0)
                msg += tr("Activate %n more %2. ", "", m_overflowLicenses[lt]).arg(QnLicense::longDisplayName(lt));
        }
    }
    else {
        foreach (Qn::LicenseType lt, licenseTypes()) {
            if (m_proposedLicenses[lt] > 0)
                msg += tr("%n more %2 will be used. ", "", m_proposedLicenses[Qn::LC_Professional]).arg(QnLicense::longDisplayName(lt));;
        }
    }
    return msg;
}

void QnLicenseUsageHelper::update() {
    int recordingTotal = 0;
    int maxTotal = 0;
    int maxLicenses[Qn::LC_Count];
    foreach (Qn::LicenseType lt, licenseTypes()) {
        m_usedLicenses[lt] = calculateUsedLicenses(lt) + m_proposedLicenses[lt];
        recordingTotal += m_usedLicenses[lt];
        maxLicenses[lt] = m_licenses.totalLicenseByType(lt);
        maxTotal += maxLicenses[lt];
    }

    foreach (Qn::LicenseType lt, licenseTypes()) {
        foreach(const LicenseCompatibility& c, compatibleLicenseType) {
            if (c.child == lt)
                borrowLicenseFromClass(m_usedLicenses[c.master], maxLicenses[c.master], m_usedLicenses[lt], maxLicenses[lt]);
        }
    }

    m_isValid = true;
    foreach (Qn::LicenseType lt, licenseTypes()) {
        m_overflowLicenses[lt] = qMax(0, m_usedLicenses[lt] - maxLicenses[lt]);
        m_isValid &= (m_overflowLicenses[lt] == 0);
    }

    emit licensesChanged();
}

bool QnLicenseUsageHelper::isValid() const
{
    return m_isValid;
}

int QnLicenseUsageHelper::totalLicense(Qn::LicenseType licenseType) const {
    return m_licenses.totalLicenseByType(licenseType);
}

int QnLicenseUsageHelper::usedLicense(Qn::LicenseType licenseType) const {
    return m_usedLicenses[licenseType];
}

/************************************************************************/
/* QnCamLicenseUsageHelper                                              */
/************************************************************************/
QnCamLicenseUsageHelper::QnCamLicenseUsageHelper():
    QnLicenseUsageHelper()
{
    init();
    update();
}

QnCamLicenseUsageHelper::QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable):
    QnLicenseUsageHelper()
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
        if (!mserver || mserver->getStatus() == QnResource::Offline)
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

QList<Qn::LicenseType> QnCamLicenseUsageHelper::licenseTypes() const {
    return QList<Qn::LicenseType>()
        << Qn::LC_Trial
        << Qn::LC_Analog
        << Qn::LC_Professional
        << Qn::LC_Edge
        << Qn::LC_VMAX
        << Qn::LC_AnalogEncoder
        ;
}

int QnCamLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType) const {
    return qnResPool->activeCamerasByLicenseType(licenseType);
}

/************************************************************************/
/* QnVideoWallLicenseUsageHelper                                        */
/************************************************************************/
QnVideoWallLicenseUsageHelper::QnVideoWallLicenseUsageHelper():
    QnLicenseUsageHelper()
{
    auto updateIfNeeded = [this](const QnResourcePtr &resource) {
        if (!resource.dynamicCast<QnVideoWallResource>())
            return;
        update();
    };

    auto connectTo = [this](const QnVideoWallResourcePtr &videowall) {
        connect(videowall, &QnVideoWallResource::itemAdded,     this, &QnVideoWallLicenseUsageHelper::update);
        connect(videowall, &QnVideoWallResource::itemChanged,   this, &QnVideoWallLicenseUsageHelper::update);
        connect(videowall, &QnVideoWallResource::itemRemoved,   this, &QnVideoWallLicenseUsageHelper::update);
    };

    auto connectIfNeeded = [this, connectTo](const QnResourcePtr &resource) {
        if (QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>())
            connectTo(videowall);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   connectIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   updateIfNeeded);
    foreach (const QnVideoWallResourcePtr &videowall, qnResPool->getResources().filtered<QnVideoWallResource>())
        connectTo(videowall);

    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnVideoWallLicenseUsageHelper::update);

    update();
}

QList<Qn::LicenseType> QnVideoWallLicenseUsageHelper::licenseTypes() const {
    return QList<Qn::LicenseType>() << Qn::LC_VideoWall;
}

int QnVideoWallLicenseUsageHelper::calculateUsedLicenses(Qn::LicenseType licenseType) const {
    Q_ASSERT(licenseType == Qn::LC_VideoWall);
    int result = 0;
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems())
        result += info.data.videoWallControlSessions;

    foreach (const QnVideoWallResourcePtr &videowall, qnResPool->getResources().filtered<QnVideoWallResource>())
        result += videowall->items()->getItems().size();

    return result;
}
