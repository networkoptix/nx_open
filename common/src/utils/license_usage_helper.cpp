#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

/* Allow to use 'master' license type instead of 'child' type inf child licenses isn't enough */
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
        if (!mserver || mserver->getStatus() == QnResource::Offline)
            continue;

        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) 
            m_proposedLicenses[camera->licenseType()]++;
    }
    if (!proposedEnable) {
        for(int i = 0; i < Qn::LC_CountCameras; ++i)
            m_proposedLicenses[i] *= -1;
    }
    update();
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

void QnLicenseUsageHelper::update()
{
    int recordingTotal = 0;
    int maxTotal = 0;
    int maxLicenses[Qn::LC_CountCameras];
    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    for (int i = 0; i < Qn::LC_CountCameras; ++i) {
        m_usedLicenses[i] = qnResPool->activeCamerasByLicenseType((Qn::LicenseType) i) + m_proposedLicenses[i];
        recordingTotal += m_usedLicenses[i];
        maxLicenses[i] = licenseListHelper.totalLicenseByType(Qn::LicenseType(i));
        maxTotal += maxLicenses[i];
    }
    
    for (int i = 0; i < Qn::LC_CountCameras; ++i) {
        foreach(const LicenseCompatibility& c, compatibleLicenseType) {
            if (c.child == Qn::LicenseType(i))
                borrowLicenseFromClass(m_usedLicenses[c.master], maxLicenses[c.master], m_usedLicenses[i], maxLicenses[i]);
        }
    }

    m_isValid = true;
    for (int i = 0; i < Qn::LC_CountCameras; ++i) {
        m_overflowLicenses[i] = qMax(0, m_usedLicenses[i] - maxLicenses[i]);
        m_isValid &= (m_overflowLicenses[i] == 0);
    }

    emit updated();
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseType licenseType) const
{
    return QObject::tr("%n %2 are used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getUsageText() const 
{
    QString licenseText;
    for (int i = 0; i < Qn::LC_CountCameras; ++i) 
    {
        {
            Qn::LicenseType licenseType = Qn::LicenseType(i);
            if (totalLicense(licenseType) == 0)
                continue;
            if (!licenseText.isEmpty())
                licenseText += lit("\n");
            licenseText += getUsageText(licenseType);
        } 
    }
    return licenseText;
}

QString QnLicenseUsageHelper::getProposedUsageText(Qn::LicenseType licenseType) const
{
    return QObject::tr("%n %2 will be used out of %1.", "", m_usedLicenses[licenseType]).arg( m_licenses.totalLicenseByType(licenseType)).arg(QnLicense::longDisplayName(licenseType));
}

QString QnLicenseUsageHelper::getProposedUsageText() const
{
    QString licenseText;
    for (int i = 0; i < Qn::LC_CountCameras; ++i) 
    {
        {
            Qn::LicenseType licenseType = Qn::LicenseType(i);
            if (licenseType != Qn::LC_Professional && totalLicense(licenseType) == 0)
                continue;
            if (!licenseText.isEmpty())
                licenseText += lit("\n");
            licenseText += getProposedUsageText(licenseType);
        } 
    }
    return licenseText;
}

QString QnLicenseUsageHelper::getRequiredLicenseMsg() const
{
    QString msg;

    if (!isValid()) 
    {
        for (int i = 0; i < Qn::LC_CountCameras; ++i) {
            if (m_overflowLicenses[i] > 0)
                msg += QObject::tr("Activate %n more %2. ", "", m_overflowLicenses[i]).arg(QnLicense::longDisplayName((Qn::LicenseType)i));
        }
    }
    else {
        for (int i = 0; i < Qn::LC_CountCameras; ++i) {
            if (m_proposedLicenses[i] > 0)
                msg += QObject::tr("%n more %2 will be used. ", "", m_proposedLicenses[Qn::LC_Professional]).arg(QnLicense::longDisplayName((Qn::LicenseType)i));;
        }
    }
    return msg;
}

bool QnLicenseUsageHelper::isOverflowForCamera(QnVirtualCameraResourcePtr camera)
{
    return !camera->isScheduleDisabled() && m_overflowLicenses[camera->licenseType()];
}

bool QnLicenseUsageHelper::isValid() const
{
    return m_isValid;
}

int QnLicenseUsageHelper::totalLicense(Qn::LicenseType licenseType) const
{
    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    return licenseListHelper.totalLicenseByType(licenseType);
}

int QnLicenseUsageHelper::usedLicense(Qn::LicenseType licenseType) const
{
    return m_usedLicenses[licenseType];
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
