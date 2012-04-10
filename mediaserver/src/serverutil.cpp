#include <QString>
#include <QUuid>
#include <QDesktopServices>

#include "serverutil.h"
#include "settings.h"

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

    return guid;
}

QString getDataDirectory()
{
#ifdef Q_OS_LINUX
    return "/opt/networkoptix/mediaserver/var";
#else
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
}

