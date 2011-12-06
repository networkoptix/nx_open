#include "license.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <QtCore/QUuid>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#ifdef Q_OS_MAC
#include <IOKit/IOKitLib.h>
#endif

QT_STATIC_CONST char networkOptixRSAPublicKey[] =
        "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAMvme+H+lWnK2cofyKHb2KKQbSUda+74\n"
        "ltqbN0Stov74i0+pc5UXdvhITRFGVTqbaip8zThTzooFN4IIfYFnGR0CAwEAAQ==\n"
        "-----END PUBLIC KEY-----";


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
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography", QSettings::NativeFormat);
    hwid = settings.value("MachineGuid").toByteArray();
#endif

    hwid = hwid.trimmed();

    // If we couldn't obtain hardware id, let's use our own generated value.
    if (hwid.isEmpty())
    {
        QSettings settings;
        hwid = settings.value("install-id").toByteArray();
        if (hwid.isEmpty())
        {
            hwid = QUuid::createUuid().toString().toAscii();
            settings.setValue("install-id", hwid);
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


QnLicense::QnLicense()
{
    m_licenseData["NAME"]      = "";
    m_licenseData["SERIAL"]    = "";
    m_licenseData["HWID"]      = "";
    m_licenseData["COUNT"]     = "0";
    m_licenseData["SIGNATURE"] = "";

    m_validLicense = -1;
}

QnLicense::QnLicense(const QnLicense &other)
    : m_licenseData(other.m_licenseData),
      m_validLicense(other.m_validLicense)
{
}

QnLicense &QnLicense::operator=(const QnLicense &other)
{
    m_licenseData = other.m_licenseData;
    m_validLicense = other.m_validLicense;

    return *this;
}

QString QnLicense::name() const
{
    return QString::fromLatin1(m_licenseData.value("NAME"));
}

void QnLicense::setName(const QString &name)
{
    m_licenseData["NAME"] = name.toLatin1();

    m_validLicense = -1;
}

QString QnLicense::serialKey() const
{
    return QString::fromLatin1(m_licenseData.value("SERIAL"));
}

void QnLicense::setSerialKey(const QString &serialKey)
{
    m_licenseData["SERIAL"] = serialKey.toLatin1();

    m_validLicense = -1;
}

QString QnLicense::hardwareId() const
{
    return QString::fromLatin1(m_licenseData.value("HWID"));
}

void QnLicense::setHardwareId(const QString &hardwareId)
{
    m_licenseData["HWID"] = hardwareId.toLatin1();

    m_validLicense = -1;
}

QString QnLicense::signature() const
{
    return QString::fromLatin1(m_licenseData.value("SIGNATURE"));
}

void QnLicense::setSignature(const QString &signature)
{
    m_licenseData["SIGNATURE"] = signature.toLatin1();

    m_validLicense = -1;
}

int QnLicense::cameraCount() const
{
    return m_licenseData.value("COUNT").toInt();
}

void QnLicense::setCameraCount(int count)
{
    m_licenseData["COUNT"] = QByteArray::number(count);

    m_validLicense = -1;
}

bool QnLicense::isValid() const
{
    if (m_validLicense == -1)
    {
        m_validLicense = 0;

        if (!m_licenseData.value("SIGNATURE").isEmpty() && m_licenseData.value("HWID") == machineHardwareId())
        {
            QByteArray licenseString;
            licenseString += "NAME=" + m_licenseData["NAME"] + "\n";
            licenseString += "SERIAL=" + m_licenseData["SERIAL"] + "\n";
            licenseString += "HWID=" + m_licenseData["HWID"] + "\n";
            licenseString += "COUNT=" + m_licenseData["COUNT"] + "\n";

            if (isSignatureMatch(licenseString, QByteArray::fromBase64(m_licenseData["SIGNATURE"]), QByteArray(networkOptixRSAPublicKey)))
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
        licenseString += "NAME=" + m_licenseData["NAME"] + "\n";
        licenseString += "SERIAL=" + m_licenseData["SERIAL"] + "\n";
        licenseString += "HWID=" + m_licenseData["HWID"] + "\n";
        licenseString += "COUNT=" + m_licenseData["COUNT"] + "\n";
        licenseString += "SIGNATURE=" + m_licenseData["SIGNATURE"] + "\n";
    }

    return licenseString;
}

QnLicense QnLicense::fromString(const QByteArray &licenseString)
{
    QnLicense license;

    foreach (QByteArray line, licenseString.split('\n'))
    {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        const int eqPos = line.indexOf('=');
        if (eqPos != -1)
        {
            const QByteArray name = line.left(eqPos);
            const QByteArray value = line.mid(eqPos + 1);

            if (!license.m_licenseData.contains(name))
                return QnLicense();

            license.m_licenseData[name] = value;
        }
    }

    return license;
}

Q_GLOBAL_STATIC_WITH_ARGS(QnLicense, theDefaultLicense, (QnLicense::fromString(QSettings().value(QLatin1String("license")).toByteArray())))

QnLicense QnLicense::defaultLicense()
{
    return *theDefaultLicense();
}

void QnLicense::setDefaultLicense(const QnLicense &license)
{
    *theDefaultLicense() = license;
    QSettings().setValue(QLatin1String("license"), license.toString());
}

Q_GLOBAL_STATIC_WITH_ARGS(QByteArray, theMachineHardwareId, (genMachineHardwareId()))

QByteArray QnLicense::machineHardwareId()
{
    return *theMachineHardwareId();
}
