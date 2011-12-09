#ifndef _UNICLIENT_LICENSE_H_
#define _UNICLIENT_LICENSE_H_

#include <QtCore/QHash>

class QnLicense
{
public:
    QnLicense();
    QnLicense(const QnLicense &other);

    QnLicense &operator=(const QnLicense &other);

    QString name() const;
    void setName(const QString &name);

    QString serialKey() const;
    void setSerialKey(const QString &serialKey);

    QString hardwareId() const;
    void setHardwareId(const QString &hardwareId);

    QString signature() const;
    void setSignature(const QString &signature);

    int cameraCount() const;
    void setCameraCount(int count);

    bool isValid() const;

    QByteArray toString() const;
    static QnLicense fromString(const QByteArray &licenseString);

    static QnLicense defaultLicense();
    static void setDefaultLicense(const QnLicense &license);

    static QByteArray machineHardwareId();

private:
    QHash<QByteArray, QByteArray> m_licenseData;
    mutable int m_validLicense;
};

#endif // _UNICLIENT_LICENSE_H_
