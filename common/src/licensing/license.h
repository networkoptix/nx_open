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

#include "core/resource/resource_fwd.h"
#include "utils/common/latin1_array.h"
#include "utils/common/id.h"
#include "nx_ec/data/api_fwd.h"

#ifdef __APPLE__
#undef verify
#endif

const QString LICENSE_TYPE_PROFESSIONAL = lit("digital");
const QString LICENSE_TYPE_ANALOG = lit("analog"); // TODO: #Elric #EC2 TOTALLY EVIL!!!!!!!!!
const QString LICENSE_TYPE_EDGE = lit("edge");

class QnLicense {
    Q_DECLARE_TR_FUNCTIONS(QnLicense);
public:
    enum Type {
        FreeLicense,
        TrialLicense,
        AnalogLicense,
        ProfessionalLicense,
        EdgeLicense,
        TypeCount,
        Invalid
    };

    enum ErrorCode {
        NoError,
        InvalidSignature,
        InvalidHardwareID,
        InvalidBrand,
        Expired,
        InvalidType,
        TooManyLicensesPerDevice
    };

    QnLicense();
    QnLicense(const QByteArray& licenseBlock);

    void loadLicenseBlock( const QByteArray& licenseBlock );

    /**
     * Check if signature matches other fields, also check hardwareId and brand
     */
    bool isValid(ErrorCode* errCode = 0, bool isNewLicense = false) const;

    static QString errorMessage(ErrorCode errCode);

    /**
     * @returns                         Whether this license is for analog cameras.
     */
    bool isAnalog() const;

    const QString &name() const;
    const QByteArray &key() const;
    qint32 cameraCount() const;
    const QByteArray &hardwareId() const;
    const QByteArray &signature() const;

    const QString &xclass() const;
    const QString &version() const;
    const QString &brand() const;
    const QString &expiration() const; // TODO: #Ivan Passing date as a string is totally evil. Please make sure your code is easy to use!!!

    const QByteArray& rawLicense() const;

    QByteArray toString() const; 

    /**
     * \returns                         Expiration time of this license, in milliseconds since epoch, 
     *                                  or -1 if this license never expires.
     */
    qint64 expirationTime() const;
    Type type() const;
    QString typeName() const;

    static QnLicensePtr readFromStream(QTextStream &stream);

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

    void parseLicenseBlock(
        const QByteArray& licenseBlock,
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock );
    void licenseBlockFromData(
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock );
    void verify( const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock );

    QUuid findRuntimeDataByLicense() const;
};

Q_DECLARE_METATYPE(QnLicensePtr)

typedef QMap<QByteArray, QnLicensePtr> QnLicenseDict;

class QnLicenseListHelper
{
public:
    QnLicenseListHelper(const QnLicenseList& licenseList);

    QList<QByteArray> allLicenseKeys() const;
    bool haveLicenseKey(const QByteArray& key) const;
    QnLicensePtr getLicenseByKey(const QByteArray& key) const;

    /**
     * \returns                         Total number of digital cameras allowed.
     */
    int totalDigital() const {
        return totalCamerasByClass(false);
    }

    /**
     * \returns                         Total number of analog cameras allowed.
     */
    int totalAnalog() const {
        return totalCamerasByClass(true);
    }

private:
    int totalCamerasByClass(bool analog) const;

private:
    QnLicenseDict m_licenseDict;
};

Q_DECLARE_METATYPE(QnLicenseList)


/**
 * License storage which is associated with instance of Enterprise Controller (i.e. should be reloaded when switching appserver).
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

private:
    bool isLicenseMatchesCurrentSystem(const QnLicensePtr &license);
    bool addLicense_i(const QnLicensePtr &license);
    bool addLicenses_i(const QnLicenseList &licenses);
private:
    QMap<QByteArray, QnLicensePtr> m_licenseDict;
    mutable QMutex m_mutex;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
