#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

QnLicenseUsageHelper::QnLicenseUsageHelper():
    m_licenses(qnLicensePool->getLicenses()),
    m_usedDigital(0),
    m_usedAnalog(0),
    m_required(0),
    m_proposedDigital(0),
    m_proposedAnalog(0),
    m_isValid(true)
{
    update();
}

QnLicenseUsageHelper::QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable):
    m_licenses(qnLicensePool->getLicenses()),
    m_usedDigital(0),
    m_usedAnalog(0),
    m_required(0),
    m_proposedDigital(0),
    m_proposedAnalog(0),
    m_isValid(true)
{
    // update will be called inside
    propose(proposedCameras, proposedEnable);
}

void QnLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    foreach (const QnVirtualCameraResourcePtr &camera, proposedCameras) {
        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) {
            if (camera->isAnalog())
                m_proposedAnalog++;
            else
                m_proposedDigital++;
        }
    }
    if (!proposedEnable) {
        m_proposedDigital *= -1;
        m_proposedAnalog *= -1;
    }
    update();
}

void QnLicenseUsageHelper::update() {
    int activeAnalog = qnResPool->activeAnalog() + m_proposedAnalog;
    int activeDigital = qnResPool->activeDigital() + m_proposedDigital;
    int active = activeDigital + activeAnalog;

    if (totalAnalog() == 0) {
        int free = totalDigital() - active;
        m_usedDigital = active;
        m_usedAnalog = 0;
        m_required = -free;
    } else {
        m_usedAnalog = qMin(activeAnalog, totalAnalog());
        m_usedDigital = activeDigital + (activeAnalog - m_usedAnalog);
        int free = totalDigital() - m_usedDigital;
        m_required = -free;
    }
    m_isValid = m_required <= 0;
}

int QnLicenseUsageHelper::totalDigital() const {
    return 100;
    return m_licenses.totalDigital();
}

int QnLicenseUsageHelper::totalAnalog() const {
    return m_licenses.totalAnalog();
}

int QnLicenseUsageHelper::usedDigital() const {
    return m_usedDigital;
}

int QnLicenseUsageHelper::usedAnalog() const {
    return m_usedAnalog;
}

int QnLicenseUsageHelper::required() const {
    return m_required;
}

bool QnLicenseUsageHelper::isValid() const {
    return m_isValid;
}
