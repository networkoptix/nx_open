#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QSharedPointer>
#include <QString>
#include <QList>
#include <QMutex>

class QnLicense
{
public:
    QnLicense(const QString& name, const QString& key, int cameraCount, const QString& signature);

    bool isValid() const;

    const QString& name() const;
    const QString& key() const ;
    const qint32 cameraCount() const;
    const QString& signature() const;

private:
    QString m_name;
    QString m_key;
    qint32 m_cameraCount;
    QString m_signature;
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

private:
    QnLicenseList m_licenses;
    mutable QMutex m_mutex;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
