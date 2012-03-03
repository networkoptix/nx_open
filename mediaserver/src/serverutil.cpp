#include <QString>
#include <QSettings>
#include <QUuid>

#include "serverutil.h"
#include "version.h"

QString serverGuid()
{
    QSettings settings(QSettings::SystemScope, ORGANIZATION_NAME, APPLICATION_NAME);
    QString guid = settings.value("serverGuid").toString();

    if (guid.isEmpty())
    {
        if (!settings.isWritable())
        {
            return guid;
        }

        guid = QUuid::createUuid().toString();
        settings.setValue("serverGuid", guid);
    }

    return guid;
}

