#pragma once

#include <QtCore/QSharedPointer>

#include <nx/utils/software_version.h>

class QnClientInstallation;
using QnClientInstallationPtr = QSharedPointer<QnClientInstallation>;

class QnClientInstallation
{
public:
    QnClientInstallation() = default;

    bool exists() const;
    nx::utils::SoftwareVersion version() const;
    void setVersion(const nx::utils::SoftwareVersion& version);
    QString rootPath() const;
    QString binaryPath() const;
    QString executableFilePath() const;
    QString libraryPath() const;

    bool verify() const;

    static QnClientInstallationPtr installationForPath(const QString &rootPath);
    bool createInstallationDat();

    bool isNeedsVerification() const;
    void setNeedsVerification(bool f);

private:
    nx::utils::SoftwareVersion m_version;

    QString m_rootPath;
    QString m_binaryPath;
    QString m_libPath;
    bool m_needsVerification = true;
};
