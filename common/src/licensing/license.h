#ifndef QN_LICENSING_LICENSE
#define QN_LICENSING_LICENSE

#include <QByteArray>
#include <QCoreApplication>
#include <QSharedPointer>
#include <QString>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QTextStream>

class QnLicense;
typedef QSharedPointer<QnLicense> QnLicensePtr;


class QnLicense {
    Q_DECLARE_TR_FUNCTIONS(QnLicense);
public:
    enum Type {
        FreeLicense,
        TrialLicense,
        AnalogLicense,
        EnterpriseLicense,
        TypeCount
    };

    QnLicense(const QByteArray& licenseBlock);

    /**
     * Check if signature matches other fields
     */
    bool isValid(const QByteArray& hardwareId) const;

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
};


// TODO: #Elric make it an STL list. Naming a non-list a list is BAD.
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

    bool haveLicenseKey(const QByteArray& key) const;
	QnLicensePtr getLicenseByKey(const QByteArray& key) const;

private:
    int totalCamerasByClass(bool analog) const;

    QMap<QByteArray, QnLicensePtr> m_licenses;
    QByteArray m_hardwareId;
	QByteArray m_oldHardwareId;
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

    const QnLicenseList &getLicenses() const;
    void addLicense(const QnLicensePtr &license);
    void addLicenses(const QnLicenseList &licenses);
    void replaceLicenses(const QnLicenseList &licenses);

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
