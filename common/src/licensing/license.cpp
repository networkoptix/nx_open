#include "license.h"

#include <cassert>

#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <utils/common/uuid.h>
#include <QtCore/QStringList>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <utils/common/app_info.h>
#include <common/common_globals.h>
#include <utils/common/synctime.h>
#include <utils/common/product_features.h>
#include "common/common_module.h"

#include "api/runtime_info_manager.h"
#include <nx_ec/data/api_runtime_data.h>

namespace {
    const char *networkOptixRSAPublicKey = "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAN4wCk8ISwRsPH0Ev/ljnEygpL9n7PhA\n"
        "EwVi0AB6ht0hQ3sZUtM9UAGrszPJOzFfZlDB2hZ4HFyXfVZcbPxOdmECAwEAAQ==\n"
        "-----END PUBLIC KEY-----";

    // This key is introduced in v2.3 to make new license types do not work in v2.2 and earlier
    const char *networkOptixRSAPublicKey2 = "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBALiqxgrnU2hl+8DVzgXrj6u4V+5ksnR5\n"
        "vtLsDeNC9eU2aLCt0Ba4KLnuVnDDWSXQ9914i8s0KXXTM+GOHpvrChUCAwEAAQ==\n"
        "-----END PUBLIC KEY-----";

    /* One analog encoder requires one license to maintain this number of cameras. */
    const int camerasPerAnalogEncoderCount = 8;

    bool isSignatureMatch(const QByteArray &data, const QByteArray &signature, const QByteArray &publicKey)
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

        if (publicRSAKey == 0 || signature.size() != RSA_size(publicRSAKey))
            return false;

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


struct LicenseTypeInfo
{
    LicenseTypeInfo(): licenseType(Qn::LC_Count), allowedForEdge(0) {}
    LicenseTypeInfo(Qn::LicenseType licenseType,  const QnLatin1Array& className, bool allowedForEdge):
        licenseType(licenseType), className(className), allowedForEdge(allowedForEdge) {}

    Qn::LicenseType licenseType;
    QnLatin1Array className;
    bool allowedForEdge;
};

static std::array<LicenseTypeInfo, Qn::LC_Count>  licenseTypeInfo =
{
    LicenseTypeInfo(Qn::LC_Trial,           "trial",         1),
    LicenseTypeInfo(Qn::LC_Analog,          "analog",        0),
    LicenseTypeInfo(Qn::LC_Professional,    "digital",       0),
    LicenseTypeInfo(Qn::LC_Edge,            "edge",          1),
    LicenseTypeInfo(Qn::LC_VMAX,            "vmax",          0),
    LicenseTypeInfo(Qn::LC_AnalogEncoder,   "analogencoder", 0),
    LicenseTypeInfo(Qn::LC_VideoWall,       "videowall",     1)
};
} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnLicense
// -------------------------------------------------------------------------- //
QnLicense::QnLicense()
:
    m_cameraCount(0),
    m_isValid1(false),
    m_isValid2(false)
{
}

QnLicense::QnLicense(const QByteArray &licenseBlock)
    : m_rawLicense(licenseBlock),
      m_isValid1(false),
      m_isValid2(false)
{
    loadLicenseBlock( licenseBlock );
}

QnLicense::~QnLicense()
{
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

QString QnLicense::displayName() const {
    return QnLicense::displayName(type());
}

QString QnLicense::displayName(Qn::LicenseType licenseType) {
    switch (licenseType) {
    case Qn::LC_Trial:          return tr("Trial");
    case Qn::LC_Analog:         return tr("Analog");
    case Qn::LC_Professional:   return tr("Professional");
    case Qn::LC_Edge:           return tr("Edge");
    case Qn::LC_VMAX:           return tr("Vmax");
    case Qn::LC_AnalogEncoder:  return tr("Analog encoder");
    case Qn::LC_VideoWall:      return tr("Video Wall");
    default:
        break;
    }
    return QString();
}

QString QnLicense::longDisplayName() const {
    return QnLicense::longDisplayName(type());
}

QString QnLicense::longDisplayName(Qn::LicenseType licenseType) {
    switch (licenseType) {
    case Qn::LC_Trial:          return tr("Trial licenses");
    case Qn::LC_Analog:         return tr("Analog licenses");
    case Qn::LC_Professional:   return tr("Professional licenses");
    case Qn::LC_Edge:           return tr("Edge licenses");
    case Qn::LC_VMAX:           return tr("Vmax licenses");
    case Qn::LC_AnalogEncoder:  return tr("Analog encoder licenses");
    case Qn::LC_VideoWall:      return tr("Video Wall licenses");
    default:
        break;
    }
    return QString();
}

QnUuid QnLicense::serverId() const {
    for(const QnPeerRuntimeInfo& info: QnRuntimeInfoManager::instance()->items()->getItems())
    {
        if (info.data.peer.peerType != Qn::PT_Server)
            continue;

        bool hwKeyOK = info.data.mainHardwareIds.contains(m_hardwareId) || info.data.compatibleHardwareIds.contains(m_hardwareId);
        bool brandOK = m_brand.isEmpty() || (m_brand == info.data.brand);
        if (hwKeyOK && brandOK)
            return info.uuid;
    }
    return QnUuid();
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

QByteArray QnLicense::hardwareId() const
{
    return m_hardwareId;
}

QByteArray QnLicense::signature() const
{
    if (m_isValid2)
        return m_signature2;

    return m_signature;
}

QString QnLicense::xclass() const
{
    return m_class;
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

QByteArray QnLicense::rawLicense() const
{
    return m_rawLicense;
}

bool QnLicense::gotError(ErrorCode* errCode, ErrorCode errorCode) const
{
    if (errCode)
        *errCode = errorCode;
    return errorCode == NoError;
}

bool checkForEdgeBox(const QString& value)
{
    const char* EDGE_BOXES[] = {
        "isd",
        "isd_s2",
        "rpi"
    };
    QByteArray box = value.toUtf8().toLower().trimmed();
    for (size_t i = 0; i < sizeof(EDGE_BOXES) / sizeof(char*); ++i)
    {
        if (box == EDGE_BOXES[i])
            return true;
    }
    return false;
}

/* 
   >= v1.5, shoud have hwid1, hwid2 or hwid3, and have brand
   v1.4 license may have or may not have brand, depending on was activation was done before or after 1.5 is released
   We just allow empty brand for all, because we believe license is correct. 
*/
bool QnLicense::isValid(ErrorCode* errCode, ValidationMode mode) const
{
    if (!m_isValid1 && !m_isValid2 && mode != VM_CheckInfo)
        return gotError(errCode, InvalidSignature);
    
    QnPeerRuntimeInfo info = QnRuntimeInfoManager::instance()->items()->getItem(mode == VM_Regular ? serverId() : qnCommon->remoteGUID());
    if (info.uuid.isNull())
        return gotError(errCode, InvalidHardwareID); // peer where license was activated not found

    if (!m_brand.isEmpty() && m_brand != info.data.brand) 
        return gotError(errCode, InvalidBrand);

    if (expirationTime() > 0 && qnSyncTime->currentMSecsSinceEpoch() > expirationTime()) // TODO: #Elric make NEVER an INT64_MAX
        return gotError(errCode, Expired);
    
    bool isEdgeBox = checkForEdgeBox(info.data.box);
    if (isEdgeBox && !licenseTypeInfo[type()].allowedForEdge)
        return gotError(errCode, InvalidType); // strict allowed license type for EDGE devices
#if 0
    if (isEdgeBox && type() == Qn::LC_Edge) 
    {
        for(const QnLicensePtr& license: qnLicensePool->getLicenses()) {
            if (license->hardwareId() == hardwareId() && license->type() == type()) 
            {
                if (mode == VM_CheckInfo && license->key() != key())
                    return gotError(errCode, TooManyLicensesPerDevice); // Only single EDGE license per ARM device is allowed
                else if (license->key() < key())
                    return gotError(errCode, TooManyLicensesPerDevice); // Only single EDGE license per ARM device is allowed
            }
        }
    }
#endif
    return gotError(errCode, NoError);
}

QByteArray QnLicense::toString() const
{
    return m_rawLicense;
}

qint64 QnLicense::expirationTime() const {

    //return QDateTime::currentMSecsSinceEpoch() + 1000 * 1000;


    if(m_expiration.isEmpty())
        return -1;

    QDateTime result = QDateTime::fromString(m_expiration, QLatin1String("yyyy-MM-dd hh:mm:ss"));
    result.setTimeSpec(Qt::UTC); /* Expiration is stored as UTC date-time. */
    return result.toMSecsSinceEpoch();
}

QString QnLicense::errorMessage(ErrorCode errCode)
{
    switch (errCode)
    {
    case NoError:
        return QString();
    case InvalidSignature:
        return tr("Invalid signature");
    case InvalidHardwareID:
        return tr("Server with necessary hardware ID is not found");
    case InvalidBrand:
        return tr("Invalid customization");
    case Expired:
        return tr("Expired"); // license is out of date
    case InvalidType:
        return tr("Invalid type");
    case TooManyLicensesPerDevice:
        return tr("Only single license is allowed for this device");
    default:
        return tr("Unknown error");
    }

    return QString();
}

Qn::LicenseType QnLicense::type() const 
{
    if (key() == qnProductFeatures().freeLicenseKey.toLatin1())
        return Qn::LC_Trial;

    if (xclass().toLower().toUtf8() == licenseTypeInfo[Qn::LC_VideoWall].className)
        return Qn::LC_VideoWall;

    if (!expiration().isEmpty())
        return Qn::LC_Trial;
    
    for (int i = 0; i < Qn::LC_Count; ++i) {
        if (xclass().toLower().toUtf8() == licenseTypeInfo[i].className)
            return licenseTypeInfo[i].licenseType;
    }

    return Qn::LC_Professional; // default value
}

void QnLicense::parseLicenseBlock(
    const QByteArray& licenseBlock,
    QByteArray* const v1LicenseBlock,
    QByteArray* const v2LicenseBlock )
{
    int n = 0;
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
            else if (aname == "SIGNATURE2" || aname == "SIGNATURE3")
                m_signature2 = avalue;
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

void QnLicense::verify( const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock )
{
    if (isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), QByteArray(networkOptixRSAPublicKey2)) ||
        isSignatureMatch(v2LicenseBlock, QByteArray::fromBase64(m_signature2), QByteArray(networkOptixRSAPublicKey))) {
        m_isValid2 = true;
    } else if (isSignatureMatch(v1LicenseBlock, QByteArray::fromBase64(m_signature), QByteArray(networkOptixRSAPublicKey)) && m_brand.isEmpty()) {
        m_class = QLatin1String("digital");
        m_version = QLatin1String("1.4");
        m_expiration = QLatin1String("");
        m_isValid1 = true;
    }
}

// -------------------------------------------------------------------------- //
// QnLicenseListHelper
// -------------------------------------------------------------------------- //

QnLicenseListHelper::QnLicenseListHelper(const QnLicenseList& licenseList) {
    update(licenseList);
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

int QnLicenseListHelper::totalLicenseByType(Qn::LicenseType licenseType) const
{
    int result = 0;

    for (const QnLicensePtr& license: m_licenseDict.values()) 
    {
        if (license->type() == licenseType && license->isValid())
            result += license->cameraCount();
    }
    return result;
}

void QnLicenseListHelper::update(const QnLicenseList& licenseList) {
    m_licenseDict.clear();
    for (const QnLicensePtr& license: licenseList) {
        m_licenseDict[license->key()] = license;
    }
}

// -------------------------------------------------------------------------- //
// QnLicensePool
// -------------------------------------------------------------------------- //
class QnLicensePoolInstance: public QnLicensePool {};
Q_GLOBAL_STATIC(QnLicensePoolInstance, qn_licensePool_instance)

QnLicensePool::QnLicensePool(): 
    m_mutex(QMutex::Recursive)
{
    connect(&m_timer, &QTimer::timeout, this, &QnLicensePool::at_timer);
    m_timer.start(1000 * 60);
}

void QnLicensePool::at_timer()
{
    for(const QnLicensePtr& license: m_licenseDict)
    {
        QnLicense::ErrorCode errCode;
        license->isValid(&errCode);
        if (errCode == QnLicense::Expired) {
            qint64 experationDelta = qnSyncTime->currentMSecsSinceEpoch() - license->expirationTime();
            if (experationDelta < m_timer.interval()) {
                emit licensesChanged();
                break;
            }
        }
    }
}

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
    return license->isValid(0);
}

bool QnLicensePool::isLicenseValid(QnLicensePtr license, QnLicense::ErrorCode* errCode) const
{
    return license->isValid(errCode);
}

bool QnLicensePool::addLicense_i(const QnLicensePtr &license)
{
    if (!license)
        return false;

    m_licenseDict[license->key()] = license;
    // We check if m_brand is empty to allow v1.4 licenses to still work
    return isLicenseMatchesCurrentSystem(license);
}

void QnLicensePool::addLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);

    if (addLicense_i(license))
        emit licensesChanged();
}

void QnLicensePool::removeLicense(const QnLicensePtr &license)
{
    QMutexLocker locker(&m_mutex);
    m_licenseDict.remove(license->key());
    emit licensesChanged();
}

bool QnLicensePool::addLicenses_i(const QnLicenseList &licenses)
{
    bool atLeastOneAdded = false;

    for(const QnLicensePtr& license: licenses) {
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


QList<QByteArray> QnLicensePool::mainHardwareIds() const {
    return QnRuntimeInfoManager::instance()->remoteInfo().data.mainHardwareIds;
}

QList<QByteArray> QnLicensePool::compatibleHardwareIds() const {
    return QnRuntimeInfoManager::instance()->remoteInfo().data.compatibleHardwareIds;
}

QByteArray QnLicensePool::currentHardwareId() const {
    QList<QByteArray> hwIds = mainHardwareIds();
    return hwIds.isEmpty() 
        ? QByteArray() 
        : hwIds.last();
}

int QnLicensePool::camerasPerAnalogEncoder() {
    return camerasPerAnalogEncoderCount;
}
