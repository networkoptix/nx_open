#pragma once

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

#include <licensing/license_fwd.h>

#include <common/common_module_aware.h>

#include "core/resource/resource_fwd.h"
#include "nx/utils/latin1_array.h"
#include "utils/common/id.h"
#include "nx_ec/data/api_fwd.h"
#include <common/common_module_aware.h>
#include <nx_ec/data/api_license_data.h>

#ifdef __APPLE__
#undef verify
#endif

struct LicenseTypeInfo
{
    LicenseTypeInfo();
    LicenseTypeInfo(Qn::LicenseType licenseType, const QnLatin1Array& className, bool allowedForARM, bool allowedToShareChannel);

    Qn::LicenseType licenseType;
    QnLatin1Array className;
    bool allowedForARM;
    bool allowedToShareChannel;
};

class QnLicense
{
    Q_DECLARE_TR_FUNCTIONS(QnLicense);
public:
    QnLicense() = default;
    QnLicense(const QByteArray& licenseBlock);
    QnLicense(const ec2::ApiDetailedLicenseData& value);
    virtual ~QnLicense() = default;

    void loadLicenseBlock( const QByteArray& licenseBlock );

    QString name() const;
    QByteArray key() const;
    void setKey(const QByteArray &value);

    qint32 cameraCount() const;
    void setCameraCount(qint32 count);

    QString hardwareId() const;
    QByteArray signature() const;
    bool isValidSignature() const;

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
    static QnLicensePtr createFromKey(const QByteArray& key);

    QString displayName() const;
    static QString displayName(Qn::LicenseType licenseType);
    QString longDisplayName() const;
    static QString longDisplayName(Qn::LicenseType licenseType);

    static LicenseTypeInfo licenseTypeInfo(Qn::LicenseType licenseType);

protected:
    void setClass(const QString &xclass);
private:
    void parseLicenseBlock(
        const QByteArray& licenseBlock,
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock );
    void verify( const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock );


private:
    QByteArray m_rawLicense;

    QString m_name;
    QByteArray m_key;
    qint32 m_cameraCount = 0;
    QString m_hardwareId;
    QByteArray m_signature;

    QString m_class;
    QString m_version;
    QString m_brand;
    QString m_expiration;
    QByteArray m_signature2;

    // Is partial v1 license valid (signature1 is used)
    bool m_isValid1 = false;

    // Is full license valid (signature2 is used)
    bool m_isValid2 = false;
};

Q_DECLARE_METATYPE(QnLicensePtr)

class QnLicenseListHelper
{
public:
    QnLicenseListHelper();
    QnLicenseListHelper(const QnLicenseList& licenseList);

    void update(const QnLicenseList& licenseList);

    QList<QByteArray> allLicenseKeys() const;
    bool haveLicenseKey(const QByteArray& key) const;
    QnLicensePtr getLicenseByKey(const QByteArray& key) const;

    /** If validator is passed, only valid licenses are counted. */
    int totalLicenseByType(Qn::LicenseType licenseType, QnLicenseValidator* validator) const;

private:
    QnLicenseDict m_licenseDict;
};

Q_DECLARE_METATYPE(QnLicenseList)


/**
 * License storage which is associated with instance of Server (i.e. should be reloaded when switching appserver).
 */
class QnLicensePool: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    QnLicensePool(QObject* parent);

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

    QnLicenseValidator* validator() const;
    QnLicenseErrorCode validateLicense(const QnLicensePtr& license) const;
    bool isLicenseValid(const QnLicensePtr& license) const;
signals:
    void licensesChanged();

private slots:
    void at_timer();
private:
    bool addLicense_i(const QnLicensePtr &license);
    bool addLicenses_i(const QnLicenseList &licenses);
private:
    QMap<QByteArray, QnLicensePtr> m_licenseDict;
    mutable QnMutex m_mutex;
    QTimer m_timer;
    QnLicenseValidator* m_licenseValidator;
};
