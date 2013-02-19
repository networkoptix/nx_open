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

QnLicense::QnLicense(const QString &name, const QByteArray &key, int cameraCount, const QByteArray &signature)
    : m_name(name),
      m_key(key),
      m_cameraCount(cameraCount),
      m_signature(signature)
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

const QByteArray &QnLicense::signature() const
{
    return m_signature;
}

bool QnLicense::isValid(const QByteArray& hardwareId) const
{
    // Note, than we do check HWID here as we suppose m_hardwareId is real current ECS hwid
    if (!m_signature.isEmpty())
    {
        QByteArray licenseString;
        licenseString += "NAME=" + m_name.toUtf8() + "\n";
        licenseString += "SERIAL=" + m_key + "\n";
        licenseString += "HWID=" + hardwareId + "\n";
        licenseString += "COUNT=" + QString::number(m_cameraCount).toUtf8() + "\n";

        if (isSignatureMatch(licenseString, QByteArray::fromBase64(m_signature), QByteArray(networkOptixRSAPublicKey)))
            return true;
    }

	return false;
}

QByteArray QnLicense::toString() const
{
    QByteArray licenseString;

    licenseString += "NAME=" + m_name.toUtf8() + "\n";
    licenseString += "SERIAL=" + m_key + "\n";
    licenseString += "COUNT=" + QString::number(m_cameraCount).toUtf8() + "\n";
    licenseString += "SIGNATURE=" + m_signature + "\n";

    return licenseString;
}

QnLicensePtr readLicenseFromString(const QByteArray &licenseString)
{
    QString name;
    QByteArray key;
    qint32 cameraCount;
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
            else if (aname == "COUNT")
                cameraCount = avalue.toInt();
            else if (aname == "SIGNATURE")
                signature = avalue;
        }
    }

	if (!name.isEmpty() && !key.isEmpty() && cameraCount != 0 && !signature.isEmpty())
		return QnLicensePtr(new QnLicense(name, key, cameraCount, signature));
	else
		return QnLicensePtr();
}

QnLicensePtr readLicenseFromStream(QTextStream& stream)
{
	QByteArray licenseBlock;
	while (!stream.atEnd()) {
		QString line = stream.readLine();
		licenseBlock.append(line + QLatin1String("\n"));

		if (line.startsWith(QLatin1String("SIGNATURE=")))
			break;
	}

	return readLicenseFromString(licenseBlock);
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
	m_licenses.setOldHardwareId(licenses.oldHardwareId());
    m_licenses.clear();
    foreach (QnLicensePtr license, licenses.licenses())
        m_licenses.append(license);

    emit licensesChanged();
}

void QnLicensePool::addLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);

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

QnLicensePtr QnLicenseList::getLicenseByKey(const QByteArray& key) const
{
	if (m_licenses.contains(key))
		return m_licenses[key];
	else
		return QnLicensePtr();
}

QnLicensePool::QnLicensePool()
    : m_mutex(QMutex::Recursive)
{
}

