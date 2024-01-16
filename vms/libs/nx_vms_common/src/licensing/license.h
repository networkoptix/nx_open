// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/common/system_context_aware.h>
#include <utils/common/id.h>

#include "license_fwd.h"

#ifdef __APPLE__
#undef verify
#endif

struct LicenseTypeInfo
{
    Qn::LicenseType licenseType = Qn::LC_Count;
    QnLatin1Array className;
    bool allowedToShareChannel = false;
};

class NX_VMS_COMMON_API QnLicense
{
    Q_DECLARE_TR_FUNCTIONS(QnLicense);

public:

    static bool isSignatureMatch(
        const QByteArray& data, const QByteArray& signature, const QByteArray& publicKey);

    struct NX_VMS_COMMON_API RegionalSupport
    {
        /**
         * Supporter company name.
         */
        QString company;

        /**
         * Contact address. Can contain an email, a web site url or a phone number.
         */
        QString address;

        bool isValid() const;

        bool operator<(const RegionalSupport& other) const;
    };

    QnLicense() = default;
    QnLicense(const QByteArray& licenseBlock);
    QnLicense(const nx::vms::api::DetailedLicenseData& value);

    virtual ~QnLicense() = default;

    void loadLicenseBlock(const QByteArray& licenseBlock);

    QString name() const;
    QByteArray key() const;
    void setKey(const QByteArray& value);

    qint32 cameraCount() const;
    void setCameraCount(qint32 count);

    QString hardwareId() const;
    QByteArray signature() const;
    bool isValidSignature() const;

    QString xclass() const;
    QString version() const;
    QString brand() const;
    // TODO: #sivanov Passing date as a string is hard to use. Improve license server api.
    QString expiration() const;
    bool neverExpire() const;

    QString orderType() const;
    bool isSaas() const; //< Whether orderType() == "saas".

    QByteArray rawLicense() const;

    QByteArray toString() const;

    /**
     * \returns                         Expiration time of this license, in milliseconds since epoch,
     *                                  or -1 if this license never expires.
     */
    qint64 expirationTime() const;

    /**
     * Additional support info about license reseller.
     */
    RegionalSupport regionalSupport() const;

    /** Each license can be deactivated at most this number of times. */
    static constexpr int kMaximumDeactivationsCount = 3;

    /** Actual deactivations count. */
    int deactivationsCount() const;

    /** How much times this license can still be deactivated. */
    int deactivationsCountLeft() const;

    QDateTime tmpExpirationDate() const;
    void setTmpExpirationDate(const QDateTime& value);

    virtual Qn::LicenseType type() const;

    bool isInfoMode() const;

    /** These license types are system unique. */
    bool isUniqueLicenseType() const;

    /** These license types are system unique. */
    static bool isUniqueLicenseType(Qn::LicenseType licenseType);

    /** Check if license can possibly be deactivated at all. */
    bool isDeactivatable() const;

    static QnLicensePtr readFromStream(QTextStream& stream);
    static QnLicensePtr createFromKey(const QByteArray& key);

    QString displayName() const;
    static QString displayName(Qn::LicenseType licenseType);

    QString longDisplayName() const;
    static QString longDisplayName(Qn::LicenseType licenseType);

    static QString displayText(Qn::LicenseType licenseType, int count);
    static QString displayText(Qn::LicenseType licenseType, int count, int total);

    static LicenseTypeInfo licenseTypeInfo(Qn::LicenseType licenseType);
    LicenseTypeInfo licenseTypeInfo() const;

    /** Fill license v1 fields from license v2 fields. */
    static QnLicensePtr createSaasLocalRecordingLicense();

protected:
    void setClass(const QString& xclass);

private:
    void parseLicenseBlock(
        const QByteArray& licenseBlock,
        QByteArray* const v1LicenseBlock,
        QByteArray* const v2LicenseBlock);

    void verify(const QByteArray& v1LicenseBlock, const QByteArray& v2LicenseBlock);

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

    QString m_orderType;

    RegionalSupport m_regionalSupport;

    int m_deactivationsCount = 0;

    // Is partial v1 license valid (signature1 is used)
    bool m_isValid1 = false;

    // Is full license valid (signature2 is used)
    bool m_isValid2 = false;

    // Used for licenses created from Saas service.
    QDateTime m_tmpExpirationDate;
};

/**
 * License storage which is associated with instance of Server (i.e. should be reloaded when switching appserver).
 */
class NX_VMS_COMMON_API QnLicensePool:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

public:
    QnLicensePool(nx::vms::common::SystemContext* context, QObject* parent = nullptr);

    nx::vms::common::SystemContext* context() const { return m_context; }

    static int hardwareIdVersion(const QString& hwId);

    /** Number of cameras per analog encoder that require 1 license. */
    static int camerasPerAnalogEncoder();

    QnLicenseList getLicenses() const;
    QnLicensePtr findLicense(const QString& key) const;

    void addLicense(const QnLicensePtr& license);
    void addLicenses(const QnLicenseList& licenses);
    void replaceLicenses(const nx::vms::api::LicenseDataList& licenses);
    void removeLicense(const QnLicensePtr& license);

    void reset();
    bool isEmpty() const;

    QVector<QString> hardwareIds(const QnUuid& serverId) const;
    QString currentHardwareId(const QnUuid& serverId) const;

signals:
    void licensesChanged();

private:
    void addLicense_i(const QnLicensePtr& license);
    void addLicenses_i(const QnLicenseList& licenses);

private:
    QMap<QByteArray, QnLicensePtr> m_licenseDict;
    mutable nx::Mutex m_mutex;
};
