#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>

#include <core/resource/media_server_resource.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include "version.h"

static QnMediaServerResourcePtr m_server;

void syncStoragesToSettings(const QnMediaServerResourcePtr &server)
{
    const QnAbstractStorageResourceList& storages = server->getStorages();

    MSSettings::roSettings()->beginWriteArray(QLatin1String("storages"));
    MSSettings::roSettings()->remove(QLatin1String(""));
    for (int i = 0; i < storages.size(); i++) {
        QnAbstractStorageResourcePtr storage = storages.at(i);
        MSSettings::roSettings()->setArrayIndex(i);
        MSSettings::roSettings()->setValue("path", storage->getUrl());
    }

    MSSettings::roSettings()->endArray();
}

QString authKey()
{
    return MSSettings::roSettings()->value("authKey").toString();
}

static bool useAlternativeGuid = false;
void setUseAlternativeGuid(bool value)
{
    useAlternativeGuid = value;
}

QString serverGuid()
{
    static QString guid;

    if (!guid.isEmpty())
        return guid;

    QString name = useAlternativeGuid ? lit("serverGuid2") : lit("serverGuid");

    guid = MSSettings::roSettings()->value(name).toString();
    if (guid.isEmpty())
    {
        if (!MSSettings::roSettings()->isWritable())
        {
            return guid;
        }

        guid = QUuid::createUuid().toString();
        MSSettings::roSettings()->setValue(name, guid);
    }
#ifdef _TEST_TWO_SERVERS
    return guid + "test";
#endif
    return guid;
}

QString getDataDirectory()
{
    const QString& dataDirFromSettings = MSSettings::roSettings()->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(VER_LINUX_ORGANIZATION_NAME);
    QString varDirName = MSSettings::roSettings()->value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}
