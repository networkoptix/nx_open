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

QUuid serverGuid() {
    static QUuid guid;

    if (!guid.isNull())
        return guid;

    guid = QUuid(MSSettings::roSettings()->value(lit("serverGuid")).toString());

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
