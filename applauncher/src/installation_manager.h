////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <functional>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QDir>
#include <QtCore/QStringList>

#include <utils/common/software_version.h>
#include <client_installation.h>

//!Provides access to installed versions information
/*!
    \todo installation API MUST be asynchronous
*/
class InstallationManager : public QObject {
    Q_OBJECT

    //!Number of installed application versions
    Q_PROPERTY( int count READ count )

public:
    InstallationManager(QObject *parent = NULL );

    void updateInstalledVersionsInformation();

    /*! Create an empty directory with name as the latest version is.
     *  It is needed to signalize client <= 2.1 about version installed in a standard way (not in compatibility folder). */
    void createGhosts();
    void createGhostsForVersion(const QnSoftwareVersion &version);

    //!Returns number of installed versions
    int count() const;
    QnSoftwareVersion latestVersion() const;
    bool isVersionInstalled(const QnSoftwareVersion &version, bool strict = false) const;
    QList<QnSoftwareVersion> installedVersions() const;
    QnSoftwareVersion nearestInstalledVersion(const QnSoftwareVersion &version) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    QnClientInstallationPtr installationForVersion(const QnSoftwareVersion &version, bool strict = false) const;

    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString rootInstallationDirectory() const;
    //!Returns path to install \a version to or empty string writable paths for installations is not known
    QString installationDirForVersion(const QnSoftwareVersion &version) const;

    bool installZip(const QnSoftwareVersion &version, const QString &fileName);

    static bool isValidVersionName(const QString &version);

    static QString defaultDirectoryForInstallations();

private:
    mutable QMap<QnSoftwareVersion, QnClientInstallationPtr> m_installationByVersion;
    QDir m_installationsDir;
    mutable std::mutex m_mutex;
};

#endif  //INSTALLATION_MANAGER_H
