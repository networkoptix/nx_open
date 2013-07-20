#include "license.h"

#include <cassert>

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

bool QnLicense::isValid(const QByteArray& hardwareId, const QString& brand) const
{
    return (m_isValid1 || m_isValid2) && (hardwareId == m_hardwareId) && (m_brand == brand);
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

    return ProfessionalLicense;
}

QString QnLicense::typeName() const {
    switch(type()) {
    case FreeLicense:       return tr("Free");
    case TrialLicense:      return tr("Trial");
    case AnalogLicense:     return tr("Analog");
    case ProfessionalLicense:   return tr("Professional");
    default:
        assert(false);
        return QString();
    }
}

// -------------------------------------------------------------------------- //
// QnLicenseListHelper
// -------------------------------------------------------------------------- //

QnLicenseListHelper::QnLicenseListHelper(const QnLicenseList& licenseList) {
    foreach (QnLicensePtr license, licenseList) {
        m_licenseDict[license->key()] = license;
    }
}

bool QnLicenseListHelper::haveLicenseKey(const QByteArray &key) const {
    return m_licenseDict.contains(key);
}

QnLicensePtr QnLicenseListHelper::getLicenseByKey(const QByteArray& key) const {
    if (m_licenseDict.contains(key))
        return m_licenseDict[key];
    else
        return QnLicensePtr();
}

QList<QByteArray> QnLicenseListHelper::allLicenseKeys() const {
    return m_licenseDict.keys();
}

int QnLicenseListHelper::totalCamerasByClass(bool analog) const
{
    int result = 0;

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    foreach (QnLicensePtr license, m_licenseDict.values())
        if (license->isAnalog() == analog && (license->expirationTime() < 0 || currentTime < license->expirationTime())) // TODO: #Elric make NEVER an INT64_MAX
            result += license->cameraCount();

    return result;
}

#if 0

void QnLicenseList::append(QnLicensePtr license)
{
    if (m_licenses.contains(license->key())) {
        // Update if resulting license is valid with current hardwareId
        if (license->isValid(m_hardwareId2))
            m_licenses[license->key()] = license;

        return;
    }

    // We allow adding any valid license by now.
    // So, user can game us as follows:
    // 1. Activate license with v1.4, 
    //    activation will be bound to the old hardware id, which is stored in registry
    // 2. Clone the system
    // 3. Re-Activate the license on all clones
    if (license->isValid(m_hardwareId2) || license->isValid(m_hardwareId1) || license->isValid(m_oldHardwareId))
        m_licenses.insert(license->key(), license);
}

void QnLicenseList::append(QnLicenseList licenses)
{
    foreach (QnLicensePtr license, licenses.m_licenses.values())
        append(license);
}

#endif // 0

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

QnLicenseList QnLicensePool::getLicenses() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenseDict.values();
}

bool QnLicensePool::isLicenseMatchesCurrentSystem(const QnLicensePtr &license) {
    const QString brand(QLatin1String(QN_PRODUCT_NAME_SHORT));

    // >= v1.5, shoud have hwid1 or hwid2, and have brand
    if (license->isValid(m_hardwareId1, brand) || license->isValid(m_hardwareId2, brand))
        return true;

    // v1.4 license may have or may not have brand, depending on was activation was done before or after 1.5 is released
    if (license->isValid(m_oldHardwareId, brand) || license->isValid(m_oldHardwareId, QLatin1String("")))
        return true;

    return false;
}

bool QnLicensePool::addLicense_i(const QnLicensePtr &license)
{
    // We check if m_brand is empty to allow v1.4 licenses to still work
    if (license && isLicenseMatchesCurrentSystem(license)) {
        m_licenseDict[license->key()] = license;
        return true;
    }

    return false;
}

void QnLicensePool::addLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);

    if (addLicense_i(license))
        emit licensesChanged();
}

bool QnLicensePool::addLicenses_i(const QnLicenseList &licenses)
{
    bool atLeastOneAdded = false;

    foreach(QnLicensePtr license, licenses) {
        if (addLicense_i(license))
            atLeastOneAdded = true;
    }

    return atLeastOneAdded;
}

void QnLicensePool::addLicenses(const QnLicenseList &licenses)
{
    QMutexLocker locker(&m_mutex);

    if (addLicenses_i(licenses))
        emit licensesChanged();
}

void QnLicensePool::replaceLicenses(const QnLicenseList &licenses)
{
    QMutexLocker locker(&m_mutex);

    m_licenseDict.clear();
    addLicenses_i(licenses);

    emit licensesChanged();
}

void QnLicensePool::reset()
{
    QMutexLocker locker(&m_mutex);

    m_licenseDict = QnLicenseDict();

    emit licensesChanged();
}

bool QnLicensePool::isEmpty() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenseDict.isEmpty();
}

void QnLicensePool::setHardwareId1(const QByteArray &hardwareId1)
{
    m_hardwareId1 = hardwareId1;
}

QByteArray QnLicensePool::hardwareId1() const
{
    return m_hardwareId1;
}

void QnLicensePool::setOldHardwareId(const QByteArray &oldHardwareId)
{
    m_oldHardwareId = oldHardwareId;
}

QByteArray QnLicensePool::oldHardwareId() const
{
    return m_oldHardwareId;
}

void QnLicensePool::setHardwareId2(const QByteArray &hardwareId2)
{
    m_hardwareId2 = hardwareId2;
}

QByteArray QnLicensePool::hardwareId2() const
{
    return m_hardwareId2;
}


