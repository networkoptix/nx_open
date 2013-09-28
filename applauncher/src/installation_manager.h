////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <map>
#include <functional>

#include <QObject>


//!Provides access to installed versions, installs new one
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
        QString installationDirectory;

        AppData()
        {
        }
        AppData( const QString& _installationDirectory )
        :
            installationDirectory( _installationDirectory )
        {
        }
    };

    InstallationManager( QObject* const parent = NULL );

    int count() const;
    QString getMostRecentVersion() const;
    bool isVersionInstalled( const QString& version ) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    bool getInstalledVersionData( const QString& version, AppData* const appData ) const;
    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    QString getRootInstallDirectory() const;
    //!Returns path to install \a version to
    QString getInstallDirForVersion( const QString& version ) const;
    //!Returns previous error description
    QString errorString() const;

    static bool isValidVersionName( const QString& version );

private:
    QString m_errorString;
    //!map<version, AppData>. Most recent version first
    std::map<QString, AppData, std::greater<QString> > m_installedProductsByVersion;
    QString m_rootInstallDirectory;

    void setErrorString( const QString& _errorString );
    void readInstalledVersions();
};

#endif  //INSTALLATION_MANAGER_H
