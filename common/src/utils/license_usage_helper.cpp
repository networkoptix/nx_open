#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

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
    // update will be called inside
    propose(proposedCameras, proposedEnable);
}

void QnLicenseUsageHelper::init()
{
    m_usedEdge = 0;
    m_usedDigital = 0;
    m_usedAnalog = 0;
    
    m_edgeOverflow = 0;
    m_digitalOverflow = 0;
    m_analogOverflow = 0;

    m_proposedDigital = 0;
    m_proposedAnalog = 0;
    m_proposedEdge = 0;
    m_isValid = true;
}

void QnLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    foreach (const QnVirtualCameraResourcePtr &camera, proposedCameras) {
        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) 
        {
            if (camera->isEdge())
                m_proposedEdge++;
            if (camera->isAnalog())
                m_proposedAnalog++;
            else
                m_proposedDigital++;
        }
    }
    if (!proposedEnable) {
        m_proposedDigital *= -1;
        m_proposedAnalog *= -1;
        m_proposedEdge *= -1;
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
    m_usedAnalog = qnResPool->activeCamerasByClass(Qn::LC_Analog) + m_proposedAnalog;
    m_usedDigital = qnResPool->activeCamerasByClass(Qn::LC_Digital) + m_proposedDigital;
    m_usedEdge = qnResPool->activeCamerasByClass(Qn::LC_Edge) + m_proposedEdge;
    int recordingTotal = m_usedAnalog + m_usedDigital + m_usedEdge;

    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    int maxAnalog = licenseListHelper.totalCamerasByClass(Qn::LC_Analog);
    int maxDigital = licenseListHelper.totalCamerasByClass(Qn::LC_Digital);
    int maxEdge = licenseListHelper.totalCamerasByClass(Qn::LC_Edge);
    int maxTotal = maxAnalog + maxDigital + maxEdge;


    if (m_usedDigital > maxDigital)
        borowLicenseFromClass(m_usedEdge, maxEdge, m_usedDigital, maxDigital);

    if (m_usedAnalog > maxAnalog) {
        borowLicenseFromClass(m_usedDigital, maxDigital, m_usedAnalog, maxAnalog);
        if (m_usedAnalog > maxAnalog)
            borowLicenseFromClass(m_usedEdge, maxEdge, m_usedAnalog, maxAnalog);
    }



    m_edgeOverflow = qMax(0, m_usedEdge - maxEdge);
    m_digitalOverflow = qMax(0, m_usedDigital - maxDigital);
    m_analogOverflow = qMax(0, m_usedAnalog - maxAnalog);
    
    m_isValid = !m_digitalOverflow && !m_edgeOverflow && !m_analogOverflow;
}

int QnLicenseUsageHelper::usedEdge() const {
    return m_usedEdge;
}

int QnLicenseUsageHelper::usedDigital() const {
    return m_usedDigital;
}

int QnLicenseUsageHelper::usedAnalog() const {
    return m_usedAnalog;
}

bool QnLicenseUsageHelper::isValid() const {
    return m_isValid;
}

int QnLicenseUsageHelper::totalDigital() const {
    //return 100;
    return m_licenses.totalCamerasByClass(Qn::LC_Digital);
}

int QnLicenseUsageHelper::totalAnalog() const {
    return m_licenses.totalCamerasByClass(Qn::LC_Analog);
}

int QnLicenseUsageHelper::totalEdge() const {
    return m_licenses.totalCamerasByClass(Qn::LC_Edge);
}

int QnLicenseUsageHelper::overflowDigital() const
{
    return m_digitalOverflow;
}

int QnLicenseUsageHelper::overflowAnalog() const
{
    return m_analogOverflow;
}

int QnLicenseUsageHelper::overflowEdge() const
{
    return m_edgeOverflow;
}

QString QnLicenseUsageHelper::getUsageText(Qn::LicenseClass licenseClass) const
{
    switch (licenseClass)
    {
        case Qn::LC_Digital:
            return QObject::tr("%n license(s) are used out of %1.", "", usedDigital()).arg(totalDigital());
        case Qn::LC_Analog:
            return QObject::tr("%n analog license(s) are used out of %1.", "", usedAnalog()).arg(totalAnalog());
        case Qn::LC_Edge:
            return QObject::tr("%n edge license(s) are used out of %1.", "", usedEdge()).arg(totalEdge());
        default:
            return QString();
    }
}


QString QnLicenseUsageHelper::getRequiredLicenseMsg() const
{
    QString msg;
    if (!isValid()) {
        if (overflowDigital() > 0)
            msg += QObject::tr("Activate %n more license(s). ", "", overflowDigital());
        if (overflowEdge() > 0)
            msg += QObject::tr("Activate %n more edge license(s). ", "", overflowEdge());
        if (overflowAnalog() > 0)
            msg += QObject::tr("Activate %n more analog license(s).", "", overflowAnalog());
    }
    else {
        if (m_proposedDigital > 0)
            msg += QObject::tr("%n more license(s) will be used. ", "", m_proposedDigital);
        if (m_proposedEdge > 0)
            msg += QObject::tr("%n more edge license(s) will be used. ", "", m_proposedEdge);
        if (m_proposedAnalog > 0)
            msg += QObject::tr("%n more analog license(s) will be used. ", "", m_proposedAnalog);
    }
    return msg;
}

bool QnLicenseUsageHelper::isOverflowForCamera(QnVirtualCameraResourcePtr camera)
{
    return !camera->isScheduleDisabled() && (overflowEdge() && camera->isEdge()) || (overflowDigital() && !camera->isAnalog()) || (overflowAnalog() && camera->isAnalog());
}
