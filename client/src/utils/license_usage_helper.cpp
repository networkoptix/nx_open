#include "license_usage_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

QnLicenseUsageHelper::QnLicenseUsageHelper():
    m_licenses(qnLicensePool->getLicenses()),
    m_digitalChange(0),
    m_analogChange(0)
{
}

QnLicenseUsageHelper::QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable):
    m_licenses(qnLicensePool->getLicenses()),
    m_digitalChange(0),
    m_analogChange(0)
{
    propose(proposedCameras, proposedEnable);
}

void QnLicenseUsageHelper::propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable) {
    foreach (const QnVirtualCameraResourcePtr &camera, proposedCameras) {
        // if schedule is disabled and we are enabling it
        if (camera->isScheduleDisabled() == proposedEnable) {
            if (camera->isAnalog())
                m_analogChange++;
            else
                m_digitalChange++;
        }
    }
    if (!proposedEnable) {
        m_digitalChange *= -1;
        m_analogChange *= -1;
    }
}

int QnLicenseUsageHelper::totalDigital() const {
    return m_licenses.totalDigital();
}

int QnLicenseUsageHelper::totalAnalog() const {
    return m_licenses.totalAnalog();
}

int QnLicenseUsageHelper::usedDigital() const {
    int requiredDigitalInsteadAnalog = qMax(totalAnalog() - usedAnalog(), 0);
    return qMin(requiredDigital() + requiredDigitalInsteadAnalog, totalDigital());
}

int QnLicenseUsageHelper::usedAnalog() const {
    return qMin(totalAnalog(), qnResPool->activeAnalog() + m_analogChange);
}

int QnLicenseUsageHelper::requiredDigital() const {
    return qnResPool->activeDigital() + m_digitalChange;
}

int QnLicenseUsageHelper::requiredAnalog() const {
    int freeDigital = qMax(totalDigital() - requiredDigital(), 0);
    return qMax(qnResPool->activeAnalog() + m_analogChange - freeDigital, 0);
}

int QnLicenseUsageHelper::proposedDigital() const {
    return isValid() ? usedDigital() : requiredDigital();
}

int QnLicenseUsageHelper::proposedAnalog() const {
    return isValid() ? usedAnalog() : requiredAnalog();
}

bool QnLicenseUsageHelper::isValid() const {
    return requiredDigital() <= totalDigital() && requiredAnalog() <= totalAnalog();
}
