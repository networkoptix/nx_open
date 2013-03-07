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
    QnLicense(const QByteArray& licenseBlock);

    /**
      * Check if signature matches other fields
      */
    bool isValid(const QByteArray& hardwareId) const;

    /**
     * @return True if license is for analog cameras
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
    const QString &expiration() const;

	const QByteArray& rawLicense() const;

    QByteArray toString() const;

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

typedef QSharedPointer<QnLicense> QnLicensePtr;

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

    /**
     * Count total number of digital cameras allowed.
     */
    int totalDigital() const {
        return totalCamerasByClass(false);
    }

    /**
     * Count total number of analog cameras allowed.
     */
    int totalAnalog() const {
        return totalCamerasByClass(true);
    }

    int totalCamerasByClass(bool analog) const;

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
