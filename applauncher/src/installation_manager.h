////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <functional>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QStringList>

#include <utils/common/software_version.h>


//!Provides access to installed versions information
/*!
    \todo installation API MUST be asynchronous
*/
class InstallationManager
:
    public QObject
{
    Q_OBJECT

    //!Number of installed application versions
    Q_PROPERTY( int count READ count )

public:
    struct AppData
    {
        QnSoftwareVersion m_version;

        QString m_rootPath;
        QString m_binaryPath;
        QString m_libPath;

        bool exists() const;
        QnSoftwareVersion version() const;
        QString rootPath() const;
        QString binaryPath() const;
        QString executableFilePath() const;
        QString libraryPath() const;

        bool verifyInstallation() const;
    };

    InstallationManager(QObject *parent = NULL );

    AppData getAppData(const QString &rootPath) const;

    void updateInstalledVersionsInformation();
    /*! Create an empty directory with name as the latest version is.
     *  It is needed to signalize client <= 2.1 about version installed in a standard way (not in compatibility folder). */
    void createGhosts();
    void createGhostsForVersion(const QnSoftwareVersion &version);
    //!Returns number of installed versions
    int count() const;
    QnSoftwareVersion getMostRecentVersion() const;
    bool isVersionInstalled( const QnSoftwareVersion& version ) const;
    QnSoftwareVersion nearestInstalledVersion(const QnSoftwareVersion &version) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    bool getInstalledVersionData(const QnSoftwareVersion &version, AppData* const appData ) const;
    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString getRootInstallDirectory() const;
    //!Returns path to install \a version to or empty string writable paths for installations is not known
    QString getInstallDirForVersion( const QnSoftwareVersion& version ) const;
    //!Returns previous error description
    QString errorString() const;

    bool installZip(const QnSoftwareVersion &version, const QString &fileName);

    static bool isValidVersionName( const QString& version );

    static QString defaultDirectoryForInstallations();

private:
    QString m_errorString;
    mutable QMap<QnSoftwareVersion, AppData> m_installedProductsByVersion;
    QStringList m_rootInstallDirectoryList;
    //QString m_rootInstallDirectory;
    QString m_defaultDirectoryForNewInstallations;
    mutable std::mutex m_mutex;

    void setErrorString( const QString& _errorString );
};

#endif  //INSTALLATION_MANAGER_H
