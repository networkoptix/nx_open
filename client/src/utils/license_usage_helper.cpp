#include "license_usage_helper.h"

#include <core/resource_managment/resource_pool.h>


QnLicenseUsageHelper::QnLicenseUsageHelper():
    m_licenses(qnLicensePool->getLicenses())
{
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
    return qMin(totalAnalog(), qnResPool->activeAnalog());
}

int QnLicenseUsageHelper::requiredDigital() {
    return qnResPool->activeDigital();
}

int QnLicenseUsageHelper::requiredAnalog() {
    int freeDigital = qMax(totalDigital() - requiredDigital(), 0);
    return qMax(qnResPool->activeAnalog() - freeDigital, 0);
}

bool QnLicenseUsageHelper::isValid() {
    return requiredDigital() < totalDigital() && requiredAnalog() < totalAnalog();
}
