#pragma once

#include <QtCore/QSharedPointer>

#include <nx/utils/software_version.h>

class QnClientInstallation;
using QnClientInstallationPtr = QSharedPointer<QnClientInstallation>;

class QnClientInstallation
{
public:
    bool exists() const;
    nx::utils::SoftwareVersion version() const;
    void setVersion(const nx::utils::SoftwareVersion& version);
    int protocolVersion() const;
    QString rootPath() const;
    QString binaryPath() const;
    QString executableFilePath() const;
    QString libraryPath() const;

    static QnClientInstallationPtr installationForPath(const QString& rootPath);

private:
    nx::utils::SoftwareVersion m_version;
    int m_protoVersion = 0;

    QString m_rootPath;
    QString m_binaryPath;
    QString m_libPath;
};
