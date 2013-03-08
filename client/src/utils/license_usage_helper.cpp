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

int QnLicenseUsageHelper::totalDigital() {
    return m_licenses.totalDigital();
}

int QnLicenseUsageHelper::totalAnalog() {
    return m_licenses.totalAnalog();
}

int QnLicenseUsageHelper::usedDigital() {
    int requiredDigitalInsteadAnalog = qMax(totalAnalog() - usedAnalog(), 0);
    return qMin(requiredDigital() + requiredDigitalInsteadAnalog, totalDigital());
}

int QnLicenseUsageHelper::usedAnalog() {
    return qMin(totalAnalog(), qnResPool->activeAnalog() + m_analogChange);
}

int QnLicenseUsageHelper::requiredDigital() {
    return qnResPool->activeDigital() + m_digitalChange;
}

int QnLicenseUsageHelper::requiredAnalog() {
    int freeDigital = qMax(totalDigital() - requiredDigital(), 0);
    return qMax(qnResPool->activeAnalog() + m_analogChange - freeDigital, 0);
}

bool QnLicenseUsageHelper::isValid() {
    return requiredDigital() <= totalDigital() && requiredAnalog() <= totalAnalog();
}
