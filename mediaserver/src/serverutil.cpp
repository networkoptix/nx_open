#include <QString>
#include <QUuid>
#include <QDesktopServices>
#include <QDir>

#include "serverutil.h"
#include "settings.h"
#include "version.h"

static QnMediaServerResourcePtr m_server;

QString defaultStoragePath()
{
#ifdef Q_OS_WIN
    return QDir::fromNativeSeparators(qSettings.value("mediaDir", "c:/records").toString());
#else
    return QDir::fromNativeSeparators(qSettings.value("mediaDir", "/tmp/vmsrecords").toString());
#endif
}

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

    if (storages.size() == 1) {
        qSettings.setValue("mediaDir", QDir::toNativeSeparators(storages.at(0)->getUrl()));
    }
}

QString authKey()
{
    return qSettings.value("authKey").toString();
}

QString serverGuid()
{
    QString guid = qSettings.value("serverGuid").toString();

    if (guid.isEmpty())
    {
        if (!qSettings.isWritable())
        {
            return guid;
        }

        guid = QUuid::createUuid().toString();
        qSettings.setValue("serverGuid", guid);
    }
#ifdef _TEST_TWO_SERVERS
    return guid + "test";
#endif
    return guid;
}

QString getDataDirectory()
{
#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(VER_LINUX_ORGANIZATION_NAME);
    QString varDirName = qSettings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
}

void setMediaServerResource(QnMediaServerResourcePtr server)
{
    m_server = server;
}

QnMediaServerResourcePtr getMediaServerResource()
{
    return m_server;
}
