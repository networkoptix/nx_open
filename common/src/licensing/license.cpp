#include "license.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <QtCore/QUuid>

#include <numeric>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#ifdef Q_OS_MAC
#include <IOKit/IOKitLib.h>
#endif

QT_STATIC_CONST char networkOptixRSAPublicKey[] =
        "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAN4wCk8ISwRsPH0Ev/ljnEygpL9n7PhA\n"
        "EwVi0AB6ht0hQ3sZUtM9UAGrszPJOzFfZlDB2hZ4HFyXfVZcbPxOdmECAwEAAQ==\n"
        "-----END PUBLIC KEY-----";

const QByteArray QnLicense::FREE_LICENSE_KEY = "0000-0000-0000-0001";

static inline QByteArray genMachineHardwareId()
{
    QByteArray hwid;

#ifdef Q_OS_MAC
    #define MAX_HWID_SIZE 1024

    char buf[MAX_HWID_SIZE];

    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringGetCString(uuidCf, buf, MAX_HWID_SIZE, kCFStringEncodingMacRoman);
    CFRelease(uuidCf);

    hwid = buf;
#endif

#ifdef Q_OS_WIN
    QSettings settings(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography"), QSettings::NativeFormat);
    hwid = settings.value(QLatin1String("MachineGuid")).toByteArray();
#endif

    hwid = hwid.trimmed();

    // If we couldn't obtain hardware id, let's use our own generated value.
    if (hwid.isEmpty())
    {
        QSettings settings;
        hwid = settings.value(QLatin1String("install-id")).toByteArray();
        if (hwid.isEmpty())
        {
            hwid = QUuid::createUuid().toString().toAscii();
            settings.setValue(QLatin1String("install-id"), hwid);
        }
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(hwid);

    return hash.result().toHex();
}

static bool isSignatureMatch(const QByteArray &data, const QByteArray &signature, const QByteArray &publicKey)
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

namespace {
    class QnLicensePoolInstance: public QnLicensePool {};
}

Q_GLOBAL_STATIC(QnLicensePoolInstance, qn_licensePool_instance)

QnLicense::QnLicense(const QString &name, const QByteArray &key, int cameraCount, const QByteArray &hwid, const QByteArray &signature)
    : m_name(name),
      m_key(key),
      m_cameraCount(cameraCount),
      m_hardwareId(hwid),
      m_signature(signature),
      m_validLicense(-1)

{
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

const QByteArray &QnLicense::hardwareId() const
{
    return m_hardwareId;
}

const QByteArray &QnLicense::signature() const
{
    return m_signature;
}

bool QnLicense::isValid() const
{
    if (m_validLicense == -1)
    {
        m_validLicense = 0;

        // Note, than we do not check HWID here
        if (!m_signature.isEmpty())
        {
            QByteArray licenseString;
            licenseString += "NAME=" + m_name.toUtf8() + "\n";
            licenseString += "SERIAL=" + m_key + "\n";
            licenseString += "HWID=" + m_hardwareId + "\n";
            licenseString += "COUNT=" + QString::number(m_cameraCount).toUtf8() + "\n";

            if (isSignatureMatch(licenseString, QByteArray::fromBase64(m_signature), QByteArray(networkOptixRSAPublicKey)))
                m_validLicense = 1;
        }
    }

    return m_validLicense == 1;
}

QByteArray QnLicense::toString() const
{
    QByteArray licenseString;
    if (isValid())
    {
        licenseString += "NAME=" + m_name.toUtf8() + "\n";
        licenseString += "SERIAL=" + m_key + "\n";
        licenseString += "HWID=" + m_hardwareId + "\n";
        licenseString += "COUNT=" + QString::number(m_cameraCount).toUtf8() + "\n";
        licenseString += "SIGNATURE=" + m_signature + "\n";
    }

    return licenseString;
}

QnLicense QnLicense::fromString(const QByteArray &licenseString)
{
    QString name;
    QByteArray key;
    qint32 cameraCount;
    QByteArray hwid;
    QByteArray signature;

    foreach (QByteArray line, licenseString.split('\n'))
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
                name = QString::fromUtf8(avalue);
            else if (aname == "SERIAL")
                key = avalue;
            else if (aname == "HWID")
                hwid = avalue;
            else if (aname == "COUNT")
                cameraCount = avalue.toInt();
            else if (aname == "SIGNATURE")
                signature = avalue;
        }
    }

    return QnLicense(name, key, cameraCount, hwid, signature);
}

QnLicensePool *QnLicensePool::instance()
{
    return qn_licensePool_instance();
}

const QnLicenseList &QnLicensePool::getLicenses() const
{
    QMutexLocker locker(&m_mutex);

    return m_licenses;
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
    m_licenses = licenses;

    emit licensesChanged();
}

void QnLicensePool::addLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);

    m_licenses.append(license);
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

QList<QnLicensePtr> QnLicenseList::licenses() const
{
    return m_licenses.values();
}

void QnLicenseList::setHardwareId(const QByteArray &hardwareId)
{
    m_hardwareId = hardwareId;
}

QByteArray QnLicenseList::hardwareId() const
{
    return m_hardwareId;
}

void QnLicenseList::append(QnLicensePtr license)
{
    if (m_licenses.contains(license->key()))
        return;

    if (!license->isValid())
        return;

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

int QnLicenseList::totalCameras() const
{
    int n = 0;
    foreach (QnLicensePtr license, m_licenses.values())
        n += license->cameraCount();

    return n;
}

bool QnLicenseList::haveLicenseKey(const QByteArray &key) const
{
    return m_licenses.contains(key);
}

QnLicensePool::QnLicensePool()
    : m_mutex(QMutex::Recursive)
{
}

