////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLATION_MANAGER_H
#define INSTALLATION_MANAGER_H

#include <map>

#include <QObject>


class ApplicationVersionData
{
public:
    //!e.g. 1.4.3
    QString version;
    //!x86/x64
    QString arch;
    //!windows/linux
    QString platform;

    QString toString() const;

    static ApplicationVersionData fromString( const QString& );
};

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
    ApplicationVersionData getMostRecentVersion() const;
    bool isVersionInstalled( const ApplicationVersionData& version ) const;
    /*!
        \return false, if \a version not found. Otherwise, true and \a *appData filled
    */
    bool getInstalledVersionData( const ApplicationVersionData& version, AppData* const appData ) const;
    //!installs package \a packagePath
    /*!
        If error occured, false is returned and \a errorString() returns error description
    */
    bool install( const QString& packagePath );
    //!Returns previous error description
    QString errorString() const;

private:
    QString m_errorString;
    //!map<version, AppData>
    std::map<QString, AppData> m_installedProductsByVersion;

    void setErrorString( const QString& _errorString );
    void readInstalledVersions();
    //!Returns path to "{program files dir}/Network Optix" on windows and "/opt/Network Optix" on unix
    static QString getRootInstallDirectory();
};

#endif  //INSTALLATION_MANAGER_H
