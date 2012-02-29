#include "license.h"

Q_GLOBAL_STATIC(QnLicensePool, globalLicensePool)

QnLicense::QnLicense(const QString& name, const QString& key, int cameraCount, const QString& signature)
    : m_name(name),
    m_key(key),
    m_cameraCount(cameraCount),
    m_signature(signature)
{
}

const QString& QnLicense::name() const
{
    return m_name;
}

const QString& QnLicense::key() const
{
    return m_key;
}

const qint32 QnLicense::cameraCount() const
{
    return m_cameraCount;
}

const QString& QnLicense::signature() const
{
    return m_signature;
}

bool QnLicense::isValid() const
{
    // check signature here
    return true;
}

QnLisencePool* QnLisencePool::instance()
{
    return globalLicensePool();
}

const QnLisenseList& QnLisencePool::getLicenses()
{
    return m_licenses;
}

void QnLisencePool::addLicense(const QnLicensePtr& license)
{
    m_licenses.append(license);
}

void QnLisencePool::removeLicense(const QnLicensePtr& license)
{
    m_licenses.remove(license);
}
