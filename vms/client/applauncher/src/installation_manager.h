#pragma once

#include <functional>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QDir>
#include <QtCore/QStringList>

#include <nx/utils/software_version.h>
#include <client_installation.h>
#include <nx/vms/applauncher/api/applauncher_api.h>

namespace nx::applauncher {

// TODO: installation API MUST be asynchronous
/**
 * Provides access to installed versions information.
 */
class InstallationManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count)

public:
    InstallationManager(QObject* parent = nullptr);

    using SoftwareVersion = nx::utils::SoftwareVersion;

    void updateInstalledVersionsInformation();

    /**
     * Create an empty directory with name as the latest version is.
     * It is needed to signalize client <= 2.1 about version installed in a standard way (not in
     * compatibility folder).
     */
    void createGhosts();
    void createGhostsForVersion(const nx::utils::SoftwareVersion& version);

    /** Number of installed application versions. */
    int count() const;

    /**
     * Find the latest installed version with the given protocol. If protocol is null, return the
     * latest installed version.
     */
    nx::utils::SoftwareVersion latestVersion(int protocolVersion = 0) const;

    bool isVersionInstalled(const nx::utils::SoftwareVersion &version, bool strict = false) const;
    QList<nx::utils::SoftwareVersion> installedVersions() const;
    nx::utils::SoftwareVersion nearestInstalledVersion(const nx::utils::SoftwareVersion &version) const;
    /**
     * @return false, if version is not found. Otherwise, true and fill *appData.
    */
    QnClientInstallationPtr installationForVersion(
        const nx::utils::SoftwareVersion& version, bool strict = false) const;

    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString rootInstallationDirectory() const;
    //!Returns path to install \a version to or empty string writable paths for installations is not known
    QString installationDirForVersion(const nx::utils::SoftwareVersion &version) const;

    api::ResultType installZip(const nx::utils::SoftwareVersion &version, const QString &fileName);

    static bool isValidVersionName(const QString &version);

    static QString defaultDirectoryForInstallations();

    void removeInstallation(const nx::utils::SoftwareVersion &version);

private:
    mutable QMap<nx::utils::SoftwareVersion, QnClientInstallationPtr> m_installationByVersion;
    QDir m_installationsDir;
    mutable std::mutex m_mutex;
};

} // namespace nx::applauncher
