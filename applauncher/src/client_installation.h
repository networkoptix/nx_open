#ifndef CLIENT_INSTALLATION_H
#define CLIENT_INSTALLATION_H

#include <QtCore/QSharedPointer>

#include <utils/common/software_version.h>

class QnClientInstallation;
typedef QSharedPointer<QnClientInstallation> QnClientInstallationPtr;

class QnClientInstallation {
public:
    QnClientInstallation();

    bool exists() const;
    QnSoftwareVersion version() const;
    void setVersion(const QnSoftwareVersion &version);
    QString rootPath() const;
    QString binaryPath() const;
    QString executableFilePath() const;
    QString libraryPath() const;

    bool verify() const;

    static QnClientInstallationPtr installationForPath(const QString &rootPath);
    bool createInstallationDat();

private:
    QnSoftwareVersion m_version;

    QString m_rootPath;
    QString m_binaryPath;
    QString m_libPath;
};

#endif // CLIENT_INSTALLATION_H
