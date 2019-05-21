#include "license.h"

#include <cassert>

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <api/runtime_info_manager.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <licensing/license_validator.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>

#include <nx_ec/data/api_conversion_functions.h>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/license_data.h>

using namespace nx::vms;

namespace {

static constexpr char* nxRSAPublicKey = "-----BEGIN PUBLIC KEY-----\n"
    "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAN4wCk8ISwRsPH0Ev/ljnEygpL9n7PhA\n"
    "EwVi0AB6ht0hQ3sZUtM9UAGrszPJOzFfZlDB2hZ4HFyXfVZcbPxOdmECAwEAAQ==\n"
    "-----END PUBLIC KEY-----";

// This key is introduced in v2.3 to make new license types do not work in v2.2 and earlier
static constexpr char* nxRSAPublicKey2 = "-----BEGIN PUBLIC KEY-----\n"
    "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBALiqxgrnU2hl+8DVzgXrj6u4V+5ksnR5\n"
    "vtLsDeNC9eU2aLCt0Ba4KLnuVnDDWSXQ9914i8s0KXXTM+GOHpvrChUCAwEAAQ==\n"
    "-----END PUBLIC KEY-----";

// This key is introduced in v2.4 to make iomodule and starter license types do not work in v2.3 and earlier
static constexpr char* nxRSAPublicKey3 = "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtb16Q2sJL/eZqNpfItB0\n"
    "oMdhttY9Ov21QN8PedcJm8+1t/qjVBg2c1AxJsMnX+0MH4dcbC9W2JCU+e2vMCX7\n"
    "HMUW4gpmvRtHPDhNutgyByOVJ7TXzCrHR/5xCXojiOLISdikVyP+IDYP+ATe5mM5\n"
    "GIWG1uTTaG7gwwJn2IVggBzUapRWAm3VZUpytfPaLzqucc/zuvoMSUD5K9DZqg4p\n"
    "Meu8VWFCPA7VhFKyuTtdTjrj/72WpLdlcSbARYjjqOO51KUIESXrGUEiw1Mo0OOn\n"
    "acOz/C4G+lXfFDOALsYUNeG//UibSsfLPghvcIXdC7ghMtYBIzafA/UVcQOqZWPK\n"
    "6wIDAQAB\n"
    "-----END PUBLIC KEY-----";

/* One analog encoder requires one license to maintain this number of cameras. */
static constexpr int camerasPerAnalogEncoderCount = 1;

bool isSignatureMatch(
    const QByteArray& data, const QByteArray& signature, const QByteArray& publicKey)
{
#ifdef ENABLE_SSL
    // Calculate SHA1 hash
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(data);
    QByteArray dataHash = hash.result();

    // Load RSA public key
    BIO *bp = BIO_new_mem_buf(const_cast<char *>(publicKey.data()), publicKey.size());
    RSA* publicRSAKey = PEM_read_bio_RSA_PUBKEY(bp, 0, 0, 0);
    BIO_free(bp);

    if (publicRSAKey == 0)
        return false;

    if (signature.size() != RSA_size(publicRSAKey))
    {
        RSA_free(publicRSAKey);
        return false;
    }

    // Decrypt data
    QScopedArrayPointer<unsigned char> decrypted(new unsigned char[signature.size()]);
    int ret = RSA_public_decrypt(signature.size(), (const unsigned char*)signature.data(), decrypted.data(), publicRSAKey, RSA_PKCS1_PADDING);
    RSA_free(publicRSAKey);
    if (ret == -1)
        return false;

    // Verify signature is correct
    return memcmp(decrypted.data(), dataHash.data(), ret) == 0;
#else
    // TODO: #Elric #android
    return true;
#endif
}

/** Make sure class names totally the same as on the activation server. */
static std::array<LicenseTypeInfo, Qn::LC_Count> licenseTypeInfo = {
    LicenseTypeInfo{Qn::LC_Trial, "trial", false},
    LicenseTypeInfo{Qn::LC_Analog, "analog", false},
    LicenseTypeInfo{Qn::LC_Professional, "digital", /*allowedToShareChannel*/ true},
    LicenseTypeInfo{Qn::LC_Edge, "edge", /*allowedToShareChannel*/ true},
    LicenseTypeInfo{Qn::LC_VMAX, "vmax", false},
    LicenseTypeInfo{Qn::LC_AnalogEncoder, "analogencoder", false},
    LicenseTypeInfo{Qn::LC_VideoWall, "videowall", false},
    LicenseTypeInfo{Qn::LC_IO, "iomodule", false},
    LicenseTypeInfo{Qn::LC_Start, "starter", /*allowedToShareChannel*/ true},
    LicenseTypeInfo{Qn::LC_Free, "free", /*allowedToShareChannel*/ true},
    LicenseTypeInfo{Qn::LC_Bridge, "bridge", false},
    LicenseTypeInfo{Qn::LC_Invalid, "", false},
};

} // namespace

//-------------------------------------------------------------------------------------------------
// QnLicense

QnLicense::QnLicense(const QByteArray& licenseBlock):
    m_rawLicense(licenseBlock)
{
    loadLicenseBlock(licenseBlock);
}

QnLicense::QnLicense(const api::DetailedLicenseData& value)
{
    QList<QByteArray> params{
        QByteArray("NAME=").append(value.name),
        QByteArray("SERIAL=").append(value.key),
        QByteArray("HWID=").append(value.hardwareId),
        QByteArray("COUNT=").append(QByteArray::number(value.cameraCount)),
        QByteArray("CLASS=").append(value.licenseType),
        QByteArray("VERSION=").append(value.version),
        QByteArray("BRAND=").append(value.brand),
        QByteArray("EXPIRATION=").append(value.expiration),
        QByteArray("SIGNATURE2=").append(value.signature)};

    if (!value.orderType.isEmpty())
        params << QByteArray("ORDERTYPE=").append(value.orderType);

    const auto licenseBlock = params.join('\n');
    loadLicenseBlock(licenseBlock);
}

bool QnLicense::isInfoMode() const
{
    return m_signature.isEmpty() && m_signature2.isEmpty();
}

void QnLicense::loadLicenseBlock( const QByteArray& licenseBlock )
{
    QByteArray v1LicenseBlock, v2LicenseBlock;
    parseLicenseBlock( licenseBlock, &v1LicenseBlock, &v2LicenseBlock );
    verify( v1LicenseBlock, v2LicenseBlock );
    this->m_rawLicense = licenseBlock;
}

QnLicensePtr QnLicense::readFromStream(QTextStream& stream)
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

QnLicensePtr QnLicense::createFromKey(const QByteArray& key)
{
    QnLicense* result = new QnLicense();
    result->m_key = key;
    return QnLicensePtr(result);
}

QString QnLicense::displayName() const
{
    return QnLicense::displayName(type());
}

QString QnLicense::displayName(Qn::LicenseType licenseType)
{
    switch (licenseType)
    {
        case Qn::LC_Trial:
            return tr("Time");
        case Qn::LC_Analog:
            return tr("Analog");
        case Qn::LC_Professional:
            return tr("Professional");
        case Qn::LC_Edge:
            return tr("Edge");
        case Qn::LC_VMAX:
            return tr("Vmax");
        case Qn::LC_AnalogEncoder:
            return tr("Analog Encoder");
        case Qn::LC_VideoWall:
            return tr("Video Wall");
        case Qn::LC_IO:
            return tr("I/O Module");
        case Qn::LC_Start:
            return tr("Start");
        case Qn::LC_Free:
            return tr("Free");
        case Qn::LC_Bridge:
            return tr("Bridge");
        case Qn::LC_Invalid:
            return tr("Invalid");
        default:
            return QString();
    }
}

QString QnLicense::longDisplayName() const
{
    return QnLicense::longDisplayName(type());
}

QString QnLicense::longDisplayName(Qn::LicenseType licenseType)
{
    switch (licenseType)
    {
        case Qn::LC_Trial:
            return tr("Time Licenses");
        case Qn::LC_Analog:
            return tr("Analog Licenses");
        case Qn::LC_Professional:
            return tr("Professional Licenses");
        case Qn::LC_Edge:
            return tr("Edge Licenses");
        case Qn::LC_VMAX:
            return tr("Vmax Licenses");
        case Qn::LC_AnalogEncoder:
            return tr("Analog Encoder Licenses");
        case Qn::LC_VideoWall:
            return tr("Video Wall Licenses");
        case Qn::LC_IO:
            return tr("I/O Module Licenses");
        case Qn::LC_Start:
            return tr("Start Licenses");
        case Qn::LC_Free:
            return tr("Free Licenses");
        case Qn::LC_Bridge:
            return tr("Bridge Licenses");
        case Qn::LC_Invalid:
            return tr("Invalid Licenses");
        default:
            return QString();
    }
}

QString QnLicense::displayText(Qn::LicenseType licenseType, int count)
{
    switch (licenseType)
    {
        case Qn::LC_Trial:
            return tr("%n Time Licenses", "", count);
        case Qn::LC_Analog:
            return tr("%n Analog Licenses", "", count);
        case Qn::LC_Professional:
            return tr("%n Professional Licenses", "", count);
        case Qn::LC_Edge:
            return tr("%n Edge Licenses", "", count);
        case Qn::LC_VMAX:
            return tr("%n Vmax Licenses", "", count);
        case Qn::LC_AnalogEncoder:
            return tr("%n Analog Encoder Licenses", "", count);
        case Qn::LC_VideoWall:
            return tr("%n Video Wall Licenses", "", count);
        case Qn::LC_IO:
            return tr("%n I/O Module Licenses", "", count);
        case Qn::LC_Start:
            return tr("%n Start Licenses", "", count);
        case Qn::LC_Free:
            return tr("%n Free Licenses", "", count);
        case Qn::LC_Bridge:
            return tr("%n Bridge Licenses", "", count);
        case Qn::LC_Invalid:
            return tr("%n Invalid Licenses", "", count);
        default:
            return QString();
    }
}

QString QnLicense::displayText(Qn::LicenseType licenseType, int count, int total)
{
    switch (licenseType)
    {
        case Qn::LC_Trial:
            return tr("%n/%1 Time Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Analog:
            return tr("%n/%1 Analog Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Professional:
            return tr("%n/%1 Professional Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Edge:
            return tr("%n/%1 Edge Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_VMAX:
            return tr("%n/%1 Vmax Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_AnalogEncoder:
            return tr("%n/%1 Analog Encoder Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_VideoWall:
            return tr("%n/%1 Video Wall Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_IO:
            return tr("%n/%1 I/O Module Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Start:
            return tr("%n/%1 Start Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Free:
            return tr("%n/%1 Free Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Bridge:
            return tr("%n/%1 Bridge Licenses",
                "%n will be replaced by the total count", count).arg(total);
        case Qn::LC_Invalid:
            return tr("%n/%1 Invalid Licenses",
                "%n will be replaced by the total count", count).arg(total);
        default:
            return QString();
    }
}

QString QnLicense::name() const
{
    return m_name;
}

QByteArray QnLicense::key() const
{
    return m_key;
}

void QnLicense::setKey(const QByteArray& value)
{
    m_key = value;
}

qint32 QnLicense::cameraCount() const
{
    return m_cameraCount;
}

void QnLicense::setCameraCount(qint32 count) {
    m_cameraCount = count;
}

QString QnLicense::hardwareId() const
{
    return m_hardwareId;
}

QByteArray QnLicense::signature() const
{
    if (m_isValid2)
        return m_signature2;

    return m_signature;
}

bool QnLicense::isValidSignature() const
{
    return m_isValid1 || m_isValid2;
}

QString QnLicense::xclass() const
{
    return m_class;
}

void QnLicense::setClass(const QString& xclass)
{
    m_class = xclass;
}

QString QnLicense::version() const
{
    return m_version;
}

QString QnLicense::brand() const
{
    return m_brand;
}

QString QnLicense::expiration() const
{
    return m_expiration;
}

bool QnLicense::neverExpire() const
{
    return m_expiration.isEmpty();
}

QByteArray QnLicense::rawLicense() const
{
    return m_rawLicense;
}

QByteArray QnLicense::toString() const
{
    return m_rawLicense;
}

qint64 QnLicense::expirationTime() const
{
    if (neverExpire())
        return -1;

    QDateTime result = QDateTime::fromString(m_expiration, QLatin1String("yyyy-MM-dd hh:mm:ss"));
    result.setTimeSpec(Qt::UTC); /* Expiration is stored as UTC date-time. */
    return result.toMSecsSinceEpoch();
}

Qn::LicenseType QnLicense::type() const
{
    if (key() == QnAppInfo::freeLicenseKey().toLatin1())
        return Qn::LC_Trial;

    if (xclass().toLower().toUtf8() == ::licenseTypeInfo[Qn::LC_VideoWall].className)
        return Qn::LC_VideoWall;

    if (!expiration().isEmpty() && !isSaas())
        return Qn::LC_Trial;

    for (int i = 0; i < Qn::LC_Count; ++i) {
        if (xclass().toLower().toUtf8() == ::licenseTypeInfo[i].className)
            return ::licenseTypeInfo[i].licenseType;
    }

    return Qn::LC_Invalid; // default value
}

void QnLicense::parseLicenseBlock(
    const QByteArray& licenseBlock,
    QByteArray* const v1LicenseBlock,
    QByteArray* const v2LicenseBlock )
{
    int n = 0;
    m_orderType.clear();

    for (QByteArray line: licenseBlock.split('\n'))
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
                m_hardwareId = QString::fromLatin1(avalue);
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
            else if (aname == "SIGNATURE2" || aname == "SIGNATURE3" || aname == "SIGNATURE4")
                m_signature2 = avalue;
            else if (aname == "ORDERTYPE")
                m_orderType = QString::fromUtf8(avalue);
        }

        // v1 license activation is 4 strings + signature
        if (n < 4) {
            *v1LicenseBlock += line + "\n";
        }

        // v2 license activation is 8 strings + signature
        if (n < 8) {
            *v2LicenseBlock += line + "\n";
        }

        n++;
    }
}

void QnLicense::verify(const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock)
{
    if (isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), nxRSAPublicKey3)
        || isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), nxRSAPublicKey2)
        || isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), nxRSAPublicKey))
    {
        m_isValid2 = true;
    }
    else if (isSignatureMatch(v1LicenseBlock, QByteArray::fromBase64(m_signature), nxRSAPublicKey)
        && m_brand.isEmpty())
    {
        m_class = QLatin1String("digital");
        m_version = QLatin1String("1.4");
        m_expiration = QLatin1String("");
        m_isValid1 = true;
    }
}

LicenseTypeInfo QnLicense::licenseTypeInfo(Qn::LicenseType licenseType)
{
    return ::licenseTypeInfo[licenseType];
}

QString QnLicense::orderType() const
{
    return m_orderType;
}

bool QnLicense::isSaas() const
{
    return orderType() == "saas";
}

//-------------------------------------------------------------------------------------------------
// QnLicenseListHelper

QnLicenseListHelper::QnLicenseListHelper(const QnLicenseList& licenseList)
{
    update(licenseList);
}

bool QnLicenseListHelper::haveLicenseKey(const QByteArray& key) const
{
    return m_licenseDict.contains(key);
}

QnLicensePtr QnLicenseListHelper::getLicenseByKey(const QByteArray& key) const
{
    if (m_licenseDict.contains(key))
        return m_licenseDict[key];
    else
        return QnLicensePtr();
}

QList<QByteArray> QnLicenseListHelper::allLicenseKeys() const
{
    return m_licenseDict.keys();
}

int QnLicenseListHelper::totalLicenseByType(Qn::LicenseType licenseType,
    QnLicenseValidator* validator) const
{
    return 100;

    if (licenseType == Qn::LC_Free)
        return std::numeric_limits<int>::max();

    int result = 0;

    for (const QnLicensePtr& license: m_licenseDict.values())
    {
        if (license->type() != licenseType)
            continue;

        if (validator && !validator->isValid(license))
            continue;

        result += license->cameraCount();
    }
    return result;
}

void QnLicenseListHelper::update(const QnLicenseList& licenseList)
{
    m_licenseDict.clear();
    for (const auto& license: licenseList)
        m_licenseDict[license->key()] = license;
}

//-------------------------------------------------------------------------------------------------
// QnLicensePool

QnLicensePool::QnLicensePool(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent),
    m_mutex(QnMutex::Recursive),
    m_licenseValidator(new QnLicenseValidator(commonModule(), this))
{
    if (!qApp)
        return;
    connect(&m_timer, &QTimer::timeout, this, &QnLicensePool::at_timer);
    m_timer.start(1000 * 60);
}

void QnLicensePool::at_timer()
{
    for (const auto& license: m_licenseDict)
    {
        if (validateLicense(license) == QnLicenseErrorCode::Expired)
        {
            qint64 experationDelta = qnSyncTime->currentMSecsSinceEpoch() - license->expirationTime();
            if (experationDelta < m_timer.interval())
            {
                emit licensesChanged();
                break;
            }
        }
    }
}

QnLicenseList QnLicensePool::getLicenses() const
{
    QnMutexLocker locker(&m_mutex);
    return m_licenseDict.values();
}

bool QnLicensePool::isLicenseValid(const QnLicensePtr& license) const
{
    return m_licenseValidator->isValid(license);
}

bool QnLicensePool::addLicense_i(const QnLicensePtr& license)
{
    if (!license)
        return false;

    m_licenseDict[license->key()] = license;
    // We check if m_brand is empty to allow v1.4 licenses to still work
    return isLicenseValid(license);
}

void QnLicensePool::addLicense(const QnLicensePtr& license)
{
    QnMutexLocker locker(&m_mutex);

    addLicense_i(license);
    emit licensesChanged();
}

void QnLicensePool::removeLicense(const QnLicensePtr& license)
{
    QnMutexLocker locker(&m_mutex);
    m_licenseDict.remove(license->key());
    emit licensesChanged();
}

bool QnLicensePool::addLicenses_i(const QnLicenseList& licenses)
{
    bool atLeastOneAdded = false;

    for(const QnLicensePtr& license: licenses)
    {
        if (addLicense_i(license))
            atLeastOneAdded = true;
    }

    return atLeastOneAdded;
}

void QnLicensePool::addLicenses(const QnLicenseList& licenses)
{
    QnMutexLocker locker(&m_mutex);

    addLicenses_i(licenses);
    emit licensesChanged();
}

void QnLicensePool::replaceLicenses(const api::LicenseDataList& licenses)
{
    QnLicenseList qnLicences;
    ec2::fromApiToResourceList(licenses, qnLicences);

    QnMutexLocker locker(&m_mutex);

    m_licenseDict.clear();
    addLicenses_i(qnLicences);

    emit licensesChanged();
}

void QnLicensePool::reset()
{
    QnMutexLocker locker(&m_mutex);
    m_licenseDict = QnLicenseDict();
    emit licensesChanged();
}

bool QnLicensePool::isEmpty() const
{
    QnMutexLocker locker(&m_mutex);
    return m_licenseDict.isEmpty();
}

QVector<QString> QnLicensePool::hardwareIds() const
{
    return commonModule()->runtimeInfoManager()->remoteInfo().data.hardwareIds;
}

QString QnLicensePool::currentHardwareId() const
{
    QString hardwareId;

    // hardwareIds is a ordered list
    // first come hwid1s, than hwid2s, etc..
    // We need to find first hardware id of last version
    QVector<QString> hwIds = hardwareIds();
    QString lastPrefix;
    for (QString hwid : hwIds) {
        NX_ASSERT(hwid.length() >= 2);

        QString prefix = hwid.mid(0, 2);
        if (prefix != lastPrefix) {
            lastPrefix = prefix;
            hardwareId = hwid;
        }
    }

    return hardwareId;
}

QnLicenseValidator* QnLicensePool::validator() const
{
    return m_licenseValidator;
}

QnLicenseErrorCode QnLicensePool::validateLicense(const QnLicensePtr& license) const
{
    return m_licenseValidator->validate(license);
}

int QnLicensePool::camerasPerAnalogEncoder()
{
    return camerasPerAnalogEncoderCount;
}
