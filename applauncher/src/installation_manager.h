////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <map>

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
    //!installs package \a packagePath
    /*!
        If error occured, false is returned and \a errorString() returns error description
    */
    bool install( const QString& packagePath );
    //!Returns previous error description
    QString errorString() const;

private:
    QString m_errorString;
    //!map<version, AppData>. Most recent version first
    std::map<QString, AppData, std::greater<QString> > m_installedProductsByVersion;

    void setErrorString( const QString& _errorString );
    void readInstalledVersions();
    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    static QString getRootInstallDirectory();
};

#endif  //INSTALLATION_MANAGER_H
