#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QTimer>

#include "core/resource/resource_fwd.h"
#include "utils/common/latin1_array.h"
#include "utils/common/id.h"
#include "nx_ec/data/api_fwd.h"

#ifdef __APPLE__
#undef verify
#endif

class QnLicense {
    Q_DECLARE_TR_FUNCTIONS(QnLicense);
public:

    enum ErrorCode {
        NoError,
        InvalidSignature,           /**< License digital signature is not match */
        InvalidHardwareID,          /**< Invalid hardware ID */
        InvalidBrand,               /**< License belong to other customization */
        Expired,                    /**< Expired */
        InvalidType,                /**< Such license type isn't allowed for that device. */
        TooManyLicensesPerDevice    /**< Too many licenses of this type per device */
    };

    enum ValidationMode {
        VM_Regular,
        VM_CheckInfo,
        VM_JustCreated
    };

    QnLicense();
    QnLicense(const QByteArray& licenseBlock);

    void loadLicenseBlock( const QByteArray& licenseBlock );

    /**
     * Check if signature matches other fields, also check hardwareId and brand
     */
    virtual bool isValid(ErrorCode* errCode = 0, ValidationMode mode = VM_Regular) const;

    static QString errorMessage(ErrorCode errCode);

    QString name() const;
    QByteArray key() const;
    void setKey(const QByteArray &value);

    qint32 cameraCount() const;
    void setCameraCount(qint32 count);

    QByteArray hardwareId() const;
    QByteArray signature() const;

    QString xclass() const;
    QString version() const;
    QString brand() const;
    QString expiration() const; // TODO: #Ivan Passing date as a string is totally evil. Please make sure your code is easy to use!!!

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
    QnUuid serverId() const;

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
    QByteArray m_hardwareId;
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
    QnLicenseListHelper(const QnLicenseList& licenseList);

    void update(const QnLicenseList& licenseList);

    QList<QByteArray> allLicenseKeys() const;
    bool haveLicenseKey(const QByteArray& key) const;
    QnLicensePtr getLicenseByKey(const QByteArray& key) const;
    int totalLicenseByType(Qn::LicenseType licenseType) const;

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

    QnLicenseList getLicenses() const;

    void addLicense(const QnLicensePtr &license);
    void addLicenses(const QnLicenseList &licenses);
    void replaceLicenses(const QnLicenseList &licenses);
    void removeLicense(const QnLicensePtr &license);

    void reset();
    bool isEmpty() const;

    QList<QByteArray> mainHardwareIds() const;
    QList<QByteArray> compatibleHardwareIds() const;
    QByteArray currentHardwareId() const;
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
    mutable QMutex m_mutex;
    QTimer m_timer;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
