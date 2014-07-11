#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

/* Allow to use 'master' license type instead of 'child' type inf child licenses isn't enough */
struct LiceseCompatibility 
{
    LiceseCompatibility(Qn::LicenseClass master, Qn::LicenseClass child): master(master), child(child) {}
    Qn::LicenseClass master;
    Qn::LicenseClass child;
};

static std::array<LiceseCompatibility,3> compatibleLicenseClass =
{
    LiceseCompatibility(Qn::LC_Edge,    Qn::LC_Digital),
    LiceseCompatibility(Qn::LC_Digital, Qn::LC_Analog),
    LiceseCompatibility(Qn::LC_Edge,    Qn::LC_Analog)
};


QnLicenseUsageHelper::QnLicenseUsageHelper():
    m_licenses(qnLicensePool->getLicenses())
{
    init();
    update();
}

QnLicenseUsageHelper::QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable):
    m_licenses(qnLicensePool->getLicenses())
{
    init();

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   &QnLicenseUsageHelper::at_resourcePool_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   &QnLicenseUsageHelper::at_resourcePool_resourceRemoved);
    connect(qnResPool, &QnResourcePool::statusChanged,   this,   &QnLicenseUsageHelper::at_resourcePool_statusChanged);

    // update will be called inside
    propose(proposedCameras, proposedEnable);
}

void QnLicenseUsageHelper::init()
{
    memset(m_usedLicenses, 0, sizeof(m_usedLicenses));
    memset(m_proposedLicenses, 0, sizeof(m_proposedLicenses));
    memset(m_overflowLicenses, 0, sizeof(m_overflowLicenses));
    
    m_isValid = true;
}

void QnLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    foreach (const QnVirtualCameraResourcePtr &camera, proposedCameras) 
    {
        QnResourcePtr mserver = qnResPool->getResourceById(camera->getParentId());
        if (mserver->getStatus() == QnResource::Offline)
            continue;

        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) 
            m_proposedLicenses[camera->licenseClass()]++;
    }
    if (!proposedEnable) {
        for(int i = 0; i < Qn::LC_Count; ++i)
            m_proposedLicenses[i] *= -1;
    }
    update();
}

void QnLicenseUsageHelper::borowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal)
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

void QnLicenseUsageHelper::update()
{
    int recordingTotal = 0;
    int maxTotal = 0;
    int maxLicenses[Qn::LC_Count];
    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    for (int i = 0; i < Qn::LC_Count; ++i) {
        m_usedLicenses[i] = qnResPool->activeCamerasByClass((Qn::LicenseClass) i) + m_proposedLicenses[i];
        recordingTotal += m_usedLicenses[i];
        maxLicenses[i] = licenseListHelper.totalLicenseByClass(Qn::LicenseClass(i));
        maxTotal += maxLicenses[i];
    }
    
    for (int i = 0; i < Qn::LC_Count; ++i) {
        foreach(const LiceseCompatibility& c, compatibleLicenseClass) {
            if (c.child == Qn::LicenseClass(i))
                borowLicenseFromClass(m_usedLicenses[c.master], maxLicenses[c.master], m_usedLicenses[i], maxLicenses[i]);
        }
    }

    m_isValid = true;
    for (int i = 0; i < Qn::LC_Count; ++i) {
        m_overflowLicenses[i] = qMax(0, m_usedLicenses[i] - maxLicenses[i]);
        m_isValid &= (m_overflowLicenses[i] == 0);
    }

    emit updated();
}

QString QnLicenseUsageHelper::longClassName(Qn::LicenseClass licenseClass) const
{
    switch (licenseClass) {
        case Qn::LC_Edge: return QObject::tr("edge license(s)");
        case Qn::LC_Analog: return QObject::tr("analog license(s)");
        default: return QObject::tr("license(s)");
    }
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseClass licenseClass) const
{
    return QObject::tr("%n %2 are used out of %1.", "", m_usedLicenses[licenseClass]).arg( m_licenses.totalLicenseByClass(licenseClass)).arg(longClassName(licenseClass));
}

QString QnLicenseUsageHelper::getWillUsageText(Qn::LicenseClass licenseClass) const
{
    return QObject::tr("%n %2 will be used out of %1.", "", m_usedLicenses[licenseClass]).arg( m_licenses.totalLicenseByClass(licenseClass)).arg(longClassName(licenseClass));
}


QString QnLicenseUsageHelper::getRequiredLicenseMsg() const
{
    QString msg;

    if (!isValid()) 
    {
        for (int i = 0; i < Qn::LC_Count; ++i) {
            if (m_overflowLicenses[i] > 0)
                msg += QObject::tr("Activate %n more %2. ", "", m_overflowLicenses[i]).arg(longClassName((Qn::LicenseClass)i));
        }
    }
    else {
        for (int i = 0; i < Qn::LC_Count; ++i) {
            if (m_proposedLicenses[i] > 0)
                msg += QObject::tr("%n more %2 will be used. ", "", m_proposedLicenses[Qn::LC_Digital]).arg(longClassName((Qn::LicenseClass)i));;
        }
    }
    return msg;
}

bool QnLicenseUsageHelper::isOverflowForCamera(QnVirtualCameraResourcePtr camera)
{
    return !camera->isScheduleDisabled() && m_overflowLicenses[camera->licenseClass()];
}

bool QnLicenseUsageHelper::isValid() const
{
    return m_isValid;
}

int QnLicenseUsageHelper::totalLicense(Qn::LicenseClass licenseClass) const
{
    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    return licenseListHelper.totalLicenseByClass(licenseClass);
}

int QnLicenseUsageHelper::usedLicense(Qn::LicenseClass licenseClass) const
{
    return m_usedLicenses[licenseClass];
}

void QnLicenseUsageHelper::at_resourcePool_resourceAdded(const QnResourcePtr & res)
{
    if (res.dynamicCast<QnMediaServerResource>())
        update();
}

void QnLicenseUsageHelper::at_resourcePool_resourceRemoved(const QnResourcePtr & res)
{
    if (res.dynamicCast<QnMediaServerResource>())
        update();
}

void QnLicenseUsageHelper::at_resourcePool_statusChanged(const QnResourcePtr & res)
{
    if (res.dynamicCast<QnMediaServerResource>())
        update();
}
