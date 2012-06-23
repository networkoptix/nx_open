#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QSharedPointer>
#include <QString>
#include <QList>
#include <QMutex>
#include <QSet>

class QnLicense
{
public:
    QnLicense(const QByteArray& name, const QByteArray& key, int cameraCount, const QByteArray& hardwareId, const QByteArray& signature);

    /**
      * Check if signature matches other fields
      */
    bool isValid() const;

    const QString& name() const;
    const QByteArray& key() const;
    const qint32 cameraCount() const;
    const QByteArray& hardwareId() const;
    const QByteArray& signature() const;

    QByteArray toString() const;
    static QnLicense fromString(const QByteArray &licenseString);

public:
    static const QByteArray FREE_LICENSE_KEY;

private:
    QByteArray m_name;
    QByteArray m_key;
    qint32 m_cameraCount;
    QByteArray m_signature;
    QByteArray m_hardwareId;

    mutable int m_validLicense;
};

typedef QSharedPointer<QnLicense> QnLicensePtr;

class QnLicenseList
{
public:
    void setHardwareId(const QByteArray& hardwareId);
    QByteArray hardwareId() const;

    QList<QnLicensePtr> licenses() const;
    void append(QnLicensePtr license);
    void append(QnLicenseList license);
    bool isEmpty() const;
    void clear();

    int totalCameras() const;
    bool haveLicenseKey(const QByteArray& key) const;

private:
    QMap<QByteArray, QnLicensePtr> m_licenses;
    QByteArray m_hardwareId;
};

/**
  * License storage which is associated with instance of Enterprise Controller (i.e. should be reloaded when switching appserver).
  *
  */
class QnLicensePool : public QObject
{
    Q_OBJECT

public:
    static QnLicensePool* instance();

    const QnLicenseList& getLicenses() const;
    void addLicenses(const QnLicenseList& licenses);
    void replaceLicenses(const QnLicenseList& licenses);
    void addLicense(const QnLicensePtr&);

    void reset();
    bool isEmpty() const;

signals:
    void licensesChanged();

protected:
    QnLicensePool();

private:
    QnLicenseList m_licenses;
    mutable QMutex m_mutex;
};

#define qnLicensePool QnLicensePool::instance()

#endif // QN_LICENSING_LICENSE
