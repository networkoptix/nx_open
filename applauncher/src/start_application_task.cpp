////////////////////////////////////////////////////////////
// 15 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "start_application_task.h"

#include <QList>


StartApplicationTask::StartApplicationTask()
{
}

StartApplicationTask::StartApplicationTask(
    const QString& _version,
    const QString& _appParams )
:
    version( _version ),
    appArgs( _appParams )
{
}

QByteArray StartApplicationTask::serialize() const
{
    return QString::fromLatin1("run\n%1\n%2\n\n").arg(version).arg(appArgs).toLatin1();
}

bool StartApplicationTask::deserialize( const QByteArray& data )
{
    const QList<QByteArray>& lines = data.split('\n');
    if( lines.size() < 3 )
        return false;
    if( lines[0] != "run" )
        return false;
    version = lines[1];
    appArgs = lines[2];

    return true;
}
