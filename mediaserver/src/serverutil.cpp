#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>

#include "serverutil.h"
#include "settings.h"
#include "version.h"

static QnMediaServerResourcePtr m_server;

void syncStoragesToSettings(QnMediaServerResourcePtr server)
{
    const QnAbstractStorageResourceList& storages = server->getStorages();

    qSettings.beginWriteArray(QLatin1String("storages"));
    qSettings.remove(QLatin1String(""));
    for (int i = 0; i < storages.size(); i++) {
        QnAbstractStorageResourcePtr storage = storages.at(i);
        qSettings.setArrayIndex(i);
        qSettings.setValue("path", storage->getUrl());
    }

    qSettings.endArray();
}

QString authKey()
{
    return qSettings.value("authKey").toString();
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

    guid = qSettings.value(name).toString();
    if (guid.isEmpty())
    {
        if (!qSettings.isWritable())
        {
            return guid;
        }

        guid = QUuid::createUuid().toString();
        qSettings.setValue(name, guid);
    }
#ifdef _TEST_TWO_SERVERS
    return guid + "test";
#endif
    return guid;
}

QString getDataDirectory()
{
    const QString& dataDirFromSettings = qSettings.value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(VER_LINUX_ORGANIZATION_NAME);
    QString varDirName = qSettings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}
