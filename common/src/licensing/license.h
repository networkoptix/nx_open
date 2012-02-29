#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QSharedPointer>
#include <QString>
#include <QList>

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

class QnLicensePool
{
public:
    static QnLicensePool* instance();

    const QnLisenseList& getLicenses();
    void addLicense(const QnLicensePtr&);
    void removeLicense(const QnLicensePtr&);

private:
    QnLisenseList m_licenses;
};

typedef QSharedPointer<QnLicense> QnLicensePtr;
typedef QList<QnLicensePtr> QnLisenseList;

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE