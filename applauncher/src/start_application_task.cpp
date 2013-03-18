////////////////////////////////////////////////////////////
// 15 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "start_application_task.h"


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
    return (version + ";" + appArgs).toLatin1();
}

bool StartApplicationTask::deserialize( const QByteArray& data )
{
    //TODO/IMPL
    return false;
}
