////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////
#pragma once

#include <functional>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QDir>
#include <QtCore/QStringList>

#include <client_installation.h>

#include <nx/utils/software_version.h>

//!Provides access to installed versions information
/*!
    \todo installation API MUST be asynchronous
*/
class InstallationManager: public QObject
{
    Q_OBJECT

    //!Number of installed application versions
    Q_PROPERTY(int count READ count)

public:
    InstallationManager(QObject* parent = nullptr);

    using SoftwareVersion = nx::utils::SoftwareVersion;

    void updateInstalledVersionsInformation();

    /*! Create an empty directory with name as the latest version is.
     *  It is needed to signalize client <= 2.1 about version installed in a standard way
     * (not in compatibility folder). */
    void createGhosts();
    void createGhostsForVersion(const SoftwareVersion& version);

    //!Returns number of installed versions
    int count() const;
    SoftwareVersion latestVersion() const;
    bool isVersionInstalled(const SoftwareVersion& version, bool strict = false) const;
    QList<SoftwareVersion> installedVersions() const;
    SoftwareVersion nearestInstalledVersion(const SoftwareVersion& version) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    QnClientInstallationPtr installationForVersion(
        const SoftwareVersion& version, bool strict = false) const;

    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString rootInstallationDirectory() const;
    //!Returns path to install \a version to or empty string writable paths for installations is not known
    QString installationDirForVersion(const SoftwareVersion& version) const;

    bool installZip(const SoftwareVersion& version, const QString& fileName);

    static bool isValidVersionName(const QString& version);

    static QString defaultDirectoryForInstallations();

    void removeInstallation(const SoftwareVersion& version);

private:
    mutable QMap<SoftwareVersion, QnClientInstallationPtr> m_installationByVersion;
    QDir m_installationsDir;
    mutable std::mutex m_mutex;
};
