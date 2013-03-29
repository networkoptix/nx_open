#include "license.h"

#include <cassert>
#include <numeric> // TODO: #Elric where does this header come from?

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <QtCore/QUuid>
#include <QtCore/QStringList>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "version.h"
#include "common/customization.h"
#include "utils/common/synctime.h"

namespace {
    const char *networkOptixRSAPublicKey = QN_RSA_PUBLIC_KEY;

    bool isSignatureMatch(const QByteArray &data, const QByteArray &signature, const QByteArray &publicKey)
    {
        // Calculate SHA1 hash
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(data);
        QByteArray dataHash = hash.result();

        // Load RSA public key
        BIO *bp = BIO_new_mem_buf(const_cast<char *>(publicKey.data()), publicKey.size());
        RSA* publicRSAKey = PEM_read_bio_RSA_PUBKEY(bp, 0, 0, 0);
        BIO_free(bp);

        if (publicRSAKey == 0 || signature.size() != RSA_size(publicRSAKey))
            return false;

        // Decrypt data
        QScopedArrayPointer<unsigned char> decrypted(new unsigned char[signature.size()]);
        int ret = RSA_public_decrypt(signature.size(), (const unsigned char*)signature.data(), decrypted.data(), publicRSAKey, RSA_PKCS1_PADDING);
        RSA_free(publicRSAKey);

        // Verify signature is correct
        return memcmp(decrypted.data(), dataHash.data(), ret) == 0;
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnLicense
// -------------------------------------------------------------------------- //
QnLicense::QnLicense(const QByteArray &licenseBlock)
    : m_rawLicense(licenseBlock),
      m_isValid1(false),
      m_isValid2(false)
{
    QByteArray v1LicenseBlock, v2LicenseBlock;

    int n = 0;
    foreach (QByteArray line, licenseBlock.split('\n'))
    {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        const int eqPos = line.indexOf('=');
        if (eqPos != -1)
        {
            const QByteArray aname = line.left(eqPos);
            const QByteArray avalue = line.mid(eqPos + 1);

            if (aname == "NAME")
                m_name = QString::fromUtf8(avalue);
            else if (aname == "SERIAL")
                m_key = avalue;
            else if (aname == "COUNT")
                m_cameraCount = avalue.toInt();
            else if (aname == "HWID")
                m_hardwareId = avalue;
            else if (aname == "SIGNATURE")
                m_signature = avalue;
            else if (aname == "CLASS")
                m_class = QString::fromUtf8(avalue);
            else if (aname == "VERSION")
                m_version = QString::fromUtf8(avalue);
            else if (aname == "BRAND")
                m_brand = QString::fromUtf8(avalue);
            else if (aname == "EXPIRATION")
                m_expiration = QString::fromUtf8(avalue);
            else if (aname == "SIGNATURE2")
                m_signature2 = avalue;
        }

        // v1 license activation is 4 strings + signature
        if (n < 4) {
            v1LicenseBlock += line + "\n";
        }

        // v2 license activation is 8 strings + signature
        if (n < 8) {
            v2LicenseBlock += line + "\n";
        }

        n++;
    }

    // Remove trailing "\n"
//    v1LicenseBlock.chop(1);
//    v2LicenseBlock.chop(1);

    if (isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), QByteArray(networkOptixRSAPublicKey))) {
        m_isValid2 = true;
    } else if (isSignatureMatch(v1LicenseBlock, QByteArray::fromBase64(m_signature), QByteArray(networkOptixRSAPublicKey))) {
        m_class = QLatin1String("digital");
        m_brand = QLatin1String("");
        m_version = QLatin1String("1.4");
        m_expiration = QLatin1String("");
        m_isValid1 = true;
    }
}

QnLicensePtr QnLicense::readFromStream(QTextStream &stream)
{
    QByteArray licenseBlock;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty()) {
            if (!licenseBlock.isEmpty())
                return QnLicensePtr(new QnLicense(licenseBlock));
            else
                continue;
        }

        licenseBlock.append(line.toUtf8() + "\n");
    }

    if (licenseBlock.isEmpty())
        return QnLicensePtr();

    return QnLicensePtr(new QnLicense(licenseBlock));
}

const QString &QnLicense::name() const
{
    return m_name;
}

const QByteArray &QnLicense::key() const
{
    return m_key;
}

qint32 QnLicense::cameraCount() const
{
    return m_cameraCount;
}

const QByteArray &QnLicense::signature() const
{
    if (m_isValid2)
        return m_signature2;

    return m_signature;
}

const QString &QnLicense::xclass() const
{
    return m_class;
}

const QString &QnLicense::version() const
{
    return m_version;
}

const QString &QnLicense::brand() const
{
    return m_brand;
}

const QString &QnLicense::expiration() const
{
    return m_expiration;
}

const QByteArray& QnLicense::rawLicense() const
{
    return m_rawLicense;
}

bool QnLicense::isValid(const QByteArray& hardwareId) const
{
    return (m_isValid1 || m_isValid2) && (hardwareId == m_hardwareId);
}

bool QnLicense::isAnalog() const {
    return m_class.toLower() == QLatin1String("analog");
}

QByteArray QnLicense::toString() const
{
    return m_rawLicense;
}

qint64 QnLicense::expirationTime() const {
    if(m_expiration.isEmpty())
        return -1;

    QDateTime result = QDateTime::fromString(m_expiration, QLatin1String("yyyy-MM-dd hh:mm:ss"));
    result.setTimeSpec(Qt::UTC); /* Expiration is stored as UTC date-time. */
    return result.toMSecsSinceEpoch();
}

QnLicense::Type QnLicense::type() const {
    if (key() == qnProductFeatures().freeLicenseKey.toAscii())
        return FreeLicense;

    if (!expiration().isEmpty())
        return TrialLicense;

    if (xclass().toLower() == QLatin1String("analog"))
        return AnalogLicense;

    return EnterpriseLicense;
}

QString QnLicense::typeName() const {
    switch(type()) {
    case FreeLicense:       return tr("Free");
    case TrialLicense:      return tr("Trial");
    case AnalogLicense:     return tr("Analog");
    case EnterpriseLicense: return tr("Enterprise");
    default:
        assert(false);
        return QString();
    }
}


// -------------------------------------------------------------------------- //
// QnLicenseList
// -------------------------------------------------------------------------- //
QList<QnLicensePtr> QnLicenseList::licenses() const
{
    return m_licenses.values();
}

QList<QByteArray> QnLicenseList::allLicenseKeys() const
{
    QList<QByteArray> result;

    foreach (const QnLicensePtr& license, m_licenses.values()) {
        result.append(license->key());
    }

    return result;
}

void QnLicenseList::setHardwareId(const QByteArray &hardwareId)
{
    m_hardwareId = hardwareId;
}

QByteArray QnLicenseList::hardwareId() const
{
    return m_hardwareId;
}

void QnLicenseList::setOldHardwareId(const QByteArray &oldHardwareId)
{
    m_oldHardwareId = oldHardwareId;
}

QByteArray QnLicenseList::oldHardwareId() const
{
    return m_oldHardwareId;
}

void QnLicenseList::append(QnLicensePtr license)
{
    if (m_licenses.contains(license->key())) {
        // Update if resulting license is valid with newHardwareId
        if (license->isValid(m_hardwareId))
            m_licenses[license->key()] = license;

        return;
    }

    if (license->isValid(m_hardwareId) || license->isValid(m_oldHardwareId))
        m_licenses.insert(license->key(), license);
}

void QnLicenseList::append(QnLicenseList licenses)
{
    foreach (QnLicensePtr license, licenses.m_licenses.values())
        append(license);
}

bool QnLicenseList::isEmpty() const
{
    return m_licenses.isEmpty();
}

void QnLicenseList::clear()
{
    m_licenses.clear();
}

int QnLicenseList::totalCamerasByClass(bool analog) const
{
    int result = 0;

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    foreach (QnLicensePtr license, m_licenses.values())
        if (license->isAnalog() == analog && currentTime < license->expirationTime())
            result += license->cameraCount();

    return result;
}

bool QnLicenseList::haveLicenseKey(const QByteArray &key) const
{
    return m_licenses.contains(key);
}

QnLicensePtr QnLicenseList::getLicenseByKey(const QByteArray& key) const
{
    if (m_licenses.contains(key))
        return m_licenses[key];
    else
        return QnLicensePtr();
}


// -------------------------------------------------------------------------- //
// QnLicensePool
// -------------------------------------------------------------------------- //
class QnLicensePoolInstance: public QnLicensePool {};
Q_GLOBAL_STATIC(QnLicensePoolInstance, qn_licensePool_instance)

QnLicensePool::QnLicensePool(): 
    m_mutex(QMutex::Recursive)
{}

QnLicensePool *QnLicensePool::instance()
{
    return qn_licensePool_instance();
}

const QnLicenseList &QnLicensePool::getLicenses() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenses;
}

void QnLicensePool::addLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);

    if (license) {
        m_licenses.append(license);

        emit licensesChanged();
    }
}

void QnLicensePool::addLicenses(const QnLicenseList &licenses)
{
    QMutexLocker locker(&m_mutex);

    m_licenses.append(licenses);

    emit licensesChanged();
}

void QnLicensePool::replaceLicenses(const QnLicenseList &licenses)
{
    QMutexLocker locker(&m_mutex);

    m_licenses.setHardwareId(licenses.hardwareId());
    m_licenses.setOldHardwareId(licenses.oldHardwareId());
    m_licenses.clear();
    foreach (QnLicensePtr license, licenses.licenses())
        m_licenses.append(license);

    emit licensesChanged();
}

void QnLicensePool::reset()
{
    QMutexLocker locker(&m_mutex);

    m_licenses = QnLicenseList();

    emit licensesChanged();
}

bool QnLicensePool::isEmpty() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenses.isEmpty();
}


