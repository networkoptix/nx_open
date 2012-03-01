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

QnLicensePool* QnLicensePool::instance()
{
    return globalLicensePool();
}

const QnLicenseList& QnLicensePool::getLicenses() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenses;
}

void QnLicensePool::addLicenses(const QnLicenseList& licenses)
{
    QMutexLocker locker(&m_mutex);

    int n = licenses.size();
    m_licenses.append(licenses);
}

void QnLicensePool::addLicense(const QnLicensePtr& license)
{
    QMutexLocker locker(&m_mutex);

    m_licenses.append(license);
}

void QnLicensePool::removeLicense(const QnLicensePtr& license)
{
    QMutexLocker locker(&m_mutex);

    m_licenses.removeOne(license);
}

bool QnLicensePool::isEmpty() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenses.isEmpty();
}
