#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QSharedPointer>
#include <QString>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QTextStream>

class QnLicense
{
public:
    QnLicense(const QString& name, const QByteArray& key, int cameraCount, const QByteArray& signature);

    /**
      * Check if signature matches other fields
      */
    bool isValid(const QByteArray& hardwareId) const;

    const QString &name() const;
    const QByteArray &key() const;
    qint32 cameraCount() const;
    const QByteArray &hardwareId() const;
    const QByteArray &signature() const;

    QByteArray toString() const;

private:
    QString m_name;
    QByteArray m_key;
    qint32 m_cameraCount;
    QByteArray m_signature;
};

typedef QSharedPointer<QnLicense> QnLicensePtr;

QnLicensePtr readLicenseFromString(const QByteArray &licenseString);
QnLicensePtr readLicenseFromStream(QTextStream& stream);

class QnLicenseList
{
public:
    void setHardwareId(const QByteArray& hardwareId);
    QByteArray hardwareId() const;

    void setOldHardwareId(const QByteArray& oldHardwareId);
    QByteArray oldHardwareId() const;

    QList<QnLicensePtr> licenses() const;
	QList<QByteArray> allLicenseKeys() const;
    void append(QnLicensePtr license);
    void append(QnLicenseList license);
    bool isEmpty() const;
    void clear();

    int totalCameras() const;
    bool haveLicenseKey(const QByteArray& key) const;
	QnLicensePtr getLicenseByKey(const QByteArray& key) const;

private:
    QMap<QByteArray, QnLicensePtr> m_licenses;
    QByteArray m_hardwareId;
	QByteArray m_oldHardwareId;
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

    const QnLicenseList &getLicenses() const;
    void addLicenses(const QnLicenseList &licenses);
    void replaceLicenses(const QnLicenseList &licenses);
    void addLicense(const QnLicensePtr &license);

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
