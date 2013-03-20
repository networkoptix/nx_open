////////////////////////////////////////////////////////////
// 15 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "start_application_task.h"

#include <QList>


namespace applauncher
{
    namespace api
    {
        namespace TaskType
        {
            Value fromString( const QnByteArrayConstRef& str )
            {
                if( str == "run" )
                    return startApplication;
                else if( str == "quit" )
                    return quit;
                else
                    return invalidTaskType;
            }
        }


        BaseTask::BaseTask( TaskType::Value _type )
        :
            type( _type )
        {
        }


        bool deserializeTask( const QByteArray& str, BaseTask** ptr )
        {
            const int taskNameEnd = str.indexOf( '\n' );
            if( taskNameEnd == -1 )
                return false;
            const TaskType::Value taskType = TaskType::fromString( QnByteArrayConstRef(str, 0, taskNameEnd) );
            switch( taskType )
            {
                case TaskType::startApplication:
                    *ptr = new StartApplicationTask();
                    break;

                case TaskType::quit:
                    *ptr = new QuitTask();
                    break;

                case TaskType::invalidTaskType:
                    return false;
            }

            if( !(*ptr)->deserialize(str) )
            {
                delete *ptr;
                *ptr = NULL;
                return false;
            }

            return true;
        }

        ////////////////////////////////////////////////////////////
        //// class StartApplicationTask
        ////////////////////////////////////////////////////////////
        StartApplicationTask::StartApplicationTask()
        :
            BaseTask( TaskType::startApplication )
        {
        }

        StartApplicationTask::StartApplicationTask(
            const QString& _version,
            const QString& _appParams )
        :
            BaseTask( TaskType::startApplication ),
            version( _version ),
            appArgs( _appParams )
        {
        }

        StartApplicationTask::StartApplicationTask(
            const QString& _version,
            const QStringList& _appParams )
        :
            BaseTask( TaskType::startApplication ),
            version( _version ),
            appArgs( _appParams.join(QLatin1String(" ")) )
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
            version = QLatin1String(lines[1]);
            appArgs = QLatin1String(lines[2]);

            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class QuitTask
        ////////////////////////////////////////////////////////////
        QuitTask::QuitTask()
        :
            BaseTask( TaskType::quit )
        {
        }

        QByteArray QuitTask::serialize() const
        {
            return QByteArray( "quit\n\n" );
        }

        bool QuitTask::deserialize( const QByteArray& data )
        {
            return data == "quit\n\n";
        }
    }
}
