#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QTimer>

#include "core/resource/resource_fwd.h"
#include "nx/utils/latin1_array.h"
#include "utils/common/id.h"
#include "nx_ec/data/api_fwd.h"
#include <common/common_module_aware.h>

#ifdef __APPLE__
#undef verify
#endif

struct LicenseTypeInfo
{
    LicenseTypeInfo();
    LicenseTypeInfo(Qn::LicenseType licenseType, const QnLatin1Array& className, bool allowedForARM);

    Qn::LicenseType licenseType;
    QnLatin1Array className;
    bool allowedForARM;
};

class QnLicense
{
    Q_DECLARE_TR_FUNCTIONS(QnLicense);
public:

    enum ErrorCode {
        NoError,
        InvalidSignature,           /**< License digital signature is not match */
        InvalidHardwareID,          /**< Invalid hardware ID */
        InvalidBrand,               /**< License belong to other customization */
        Expired,                    /**< Expired */
        InvalidType,                /**< Such license type isn't allowed for that device. */
        TooManyLicensesPerDevice,   /**< Too many licenses of this type per device. */
        FutureLicense               /**< License type is unknown, may be license from future version. */
    };

    enum ValidationMode {
        VM_Regular,
        VM_CheckInfo,
        VM_JustCreated
    };

    QnLicense();
    QnLicense(const QByteArray& licenseBlock);
    virtual ~QnLicense();

    void loadLicenseBlock( const QByteArray& licenseBlock );

    /**
     * Check if signature matches other fields, also check hardwareId and brand
     */
    virtual bool isValid(
        QnCommonModule* commonModule,
        ErrorCode* errCode = 0,
        ValidationMode mode = VM_Regular) const;
    QString validationInfo(ValidationMode mode = VM_Regular) const;

    static QString errorMessage(ErrorCode errCode);

    QString name() const;
    QByteArray key() const;
    void setKey(const QByteArray &value);

    qint32 cameraCount() const;
    void setCameraCount(qint32 count);

    QString hardwareId() const;
    QByteArray signature() const;

    QString xclass() const;
    QString version() const;
    QString brand() const;
    QString expiration() const; // TODO: #Ivan Passing date as a string is totally evil. Please make sure your code is easy to use!!!
    bool neverExpire() const;

    QByteArray rawLicense() const;

    QByteArray toString() const;

    /**
     * \returns                         Expiration time of this license, in milliseconds since epoch,
     *                                  or -1 if this license never expires.
     */
    qint64 expirationTime() const;

    virtual Qn::LicenseType type() const;

    bool isInfoMode() const;

    static QnLicensePtr readFromStream(QTextStream &stream);

    QString displayName() const;
    static QString displayName(Qn::LicenseType licenseType);
    QString longDisplayName() const;
    static QString longDisplayName(Qn::LicenseType licenseType);

    /** Id of the server this license attached to (if it is present in the current system). */
    QnUuid serverId(QnCommonModule* commonModule) const;

    static LicenseTypeInfo licenseTypeInfo(Qn::LicenseType licenseType);

protected:
    bool isValidEdgeLicense(ErrorCode* errCode = 0, ValidationMode mode = VM_Regular) const;
    bool isValidStartLicense(ErrorCode* errCode = 0, ValidationMode mode = VM_Regular) const;
    bool isAllowedForArm() const;

    void setClass(const QString &xclass);
private:
    void parseLicenseBlock(
        const QByteArray& licenseBlock,
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock );
    void licenseBlockFromData(
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock );
    void verify( const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock );

    bool gotError(ErrorCode* errCode, ErrorCode errorCode) const;

private:
    QByteArray m_rawLicense;

    QString m_name;
    QByteArray m_key;
    qint32 m_cameraCount;
    QString m_hardwareId;
    QByteArray m_signature;

    QString m_class;
    QString m_version;
    QString m_brand;
    QString m_expiration;
    QByteArray m_signature2;

    // Is partial v1 license valid (signature1 is used)
    bool m_isValid1;

    // Is full license valid (signature2 is used)
    bool m_isValid2;
};

Q_DECLARE_METATYPE(QnLicensePtr)

typedef QMap<QByteArray, QnLicensePtr> QnLicenseDict;

class QnLicenseListHelper
{
public:
    QnLicenseListHelper();
    QnLicenseListHelper(const QnLicenseList& licenseList);

    void update(const QnLicenseList& licenseList);

    QList<QByteArray> allLicenseKeys() const;
    bool haveLicenseKey(const QByteArray& key) const;
    QnLicensePtr getLicenseByKey(const QByteArray& key) const;
    int totalLicenseByType(Qn::LicenseType licenseType, bool ignoreValidity = false) const;

private:

private:
    QnLicenseDict m_licenseDict;
};

Q_DECLARE_METATYPE(QnLicenseList)


/**
 * License storage which is associated with instance of Server (i.e. should be reloaded when switching appserver).
 */
class QnLicensePool : public QObject
{
    Q_OBJECT

public:
    static QnLicensePool* instance();

    /** Number of cameras per analog encoder that require 1 license. */
    static int camerasPerAnalogEncoder();

    QnLicenseList getLicenses() const;

    void addLicense(const QnLicensePtr &license);
    void addLicenses(const QnLicenseList &licenses);
    void replaceLicenses(const ec2::ApiLicenseDataList& licenses);
    void removeLicense(const QnLicensePtr &license);

    void reset();
    bool isEmpty() const;

    QVector<QString> hardwareIds() const;
    QString currentHardwareId() const;
    bool isLicenseValid(QnLicensePtr license, QnLicense::ErrorCode* errCode = 0) const;
signals:
    void licensesChanged();

protected:
    QnLicensePool();
private slots:
    void at_timer();
private:
    bool isLicenseMatchesCurrentSystem(const QnLicensePtr &license);
    bool addLicense_i(const QnLicensePtr &license);
    bool addLicenses_i(const QnLicenseList &licenses);
private:
    QMap<QByteArray, QnLicensePtr> m_licenseDict;
    mutable QnMutex m_mutex;
    QTimer m_timer;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
