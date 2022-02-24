// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

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
    bool canBeRemoved();
    QString rootPath() const;
    QString binaryPath() const;
    QString executableFilePath() const;
    QString libraryPath() const;

    static QnClientInstallationPtr installationForPath(
        const QString& rootPath, bool canBeRemoved = true);

    std::chrono::milliseconds lastExecutionTime() const;
    void updateLastExecutionTime();

private:
    nx::utils::SoftwareVersion m_version;
    int m_protoVersion = 0;
    bool m_canBeRemoved = true;

    QString m_rootPath;
    QString m_binaryPath;
    QString m_libPath;

    std::chrono::milliseconds m_lastExecutionTime{0};
};
