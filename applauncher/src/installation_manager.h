////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <functional>
#include <map>
#include <mutex>

#include <QObject>


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
        QString rootDir;
        QString installationDirectory;

        AppData()
        {
        }
        AppData(
            const QString& _rootDir,
            const QString& _installationDirectory )
        :
            rootDir( _rootDir ),
            installationDirectory( _installationDirectory )
        {
        }
    };

    InstallationManager( QObject* const parent = NULL );

    void updateInstalledVersionsInformation();
    //!Returns number of installed versions
    int count() const;
    QString getMostRecentVersion() const;
    bool isVersionInstalled( const QString& version ) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    bool getInstalledVersionData( const QString& version, AppData* const appData ) const;
    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString getRootInstallDirectory() const;
    //!Returns path to install \a version to or empty string writable paths for installations is not known
    QString getInstallDirForVersion( const QString& version ) const;
    //!Returns previous error description
    QString errorString() const;

    static bool isValidVersionName( const QString& version );

private:
    QString m_errorString;
    //!map<version, AppData>. Most recent version first
    std::map<QString, AppData, std::greater<QString> > m_installedProductsByVersion;
    std::list<QString> m_rootInstallDirectoryList;
    //QString m_rootInstallDirectory;
    QString m_defaultDirectoryForNewInstallations;
    mutable std::mutex m_mutex;

    void setErrorString( const QString& _errorString );
};

#endif  //INSTALLATION_MANAGER_H
