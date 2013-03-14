#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

QnLicenseUsageHelper::QnLicenseUsageHelper():
    m_licenses(qnLicensePool->getLicenses()),
    m_usedDigital(0),
    m_usedAnalog(0),
    m_requiredDigital(0),
    m_requiredAnalog(0),
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
    m_requiredDigital(0),
    m_requiredAnalog(0),
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
    int freeDigital = totalDigital() - activeDigital;
    if (freeDigital < 0) {
        m_isValid = false;
        m_usedDigital = activeDigital;
        m_requiredDigital = -freeDigital;

        m_usedAnalog = activeAnalog;
        m_requiredAnalog = qMax(activeAnalog - totalAnalog(), 0);
        return;
    }

    int freeAnalog = totalAnalog() + freeDigital - activeAnalog;
    if (freeAnalog < 0) {
        m_isValid = false;
        m_usedDigital = totalDigital();
        m_requiredDigital = 0;

        m_usedAnalog = activeAnalog - freeDigital;
        m_requiredAnalog = -freeAnalog;
        return;
    }

    m_isValid = true;

    int requiredDigitalInsteadAnalog = qMax(activeAnalog - totalAnalog(), 0);
    m_usedDigital = activeDigital + requiredDigitalInsteadAnalog;
    m_usedAnalog = activeAnalog - requiredDigitalInsteadAnalog;

    m_requiredDigital = m_requiredAnalog = 0;
}

int QnLicenseUsageHelper::totalDigital() const {
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

int QnLicenseUsageHelper::requiredDigital() const {
    return m_requiredDigital;
}

int QnLicenseUsageHelper::requiredAnalog() const {
    return m_requiredAnalog;
}


bool QnLicenseUsageHelper::isValid() const {
    return m_isValid;
}
