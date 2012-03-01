#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QSharedPointer>
#include <QString>
#include <QList>
#include <QMutex>

class QnLicense
{
public:
    QnLicense(const QByteArray& name, const QByteArray& key, int cameraCount, const QByteArray& hardwareId, const QByteArray& signature);

    bool isValid() const;

    const QByteArray& name() const;
    const QByteArray& key() const ;
    const qint32 cameraCount() const;
    const QByteArray& hardwareId() const;
    const QByteArray& signature() const;

    QByteArray toString() const;
    static QnLicense fromString(const QByteArray &licenseString);

private:
    QByteArray m_name;
    QByteArray m_key;
    qint32 m_cameraCount;
    QByteArray m_signature;
    QByteArray m_hardwareId;

    mutable int m_validLicense;
};

typedef QSharedPointer<QnLicense> QnLicensePtr;
typedef QList<QnLicensePtr> QnLicenseList;

class QnLicensePool
{
public:
    static QnLicensePool* instance();

    const QnLicenseList& getLicenses() const;
    void addLicenses(const QnLicenseList& licenses);
    void addLicense(const QnLicensePtr&);
    void removeLicense(const QnLicensePtr&);

    bool isEmpty() const;
    bool haveValidLicense() const;

    const QByteArray& hardwareId() const;

private:
    QByteArray m_hardwareId;
    QnLicenseList m_licenses;
    mutable QMutex m_mutex;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
