////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef APPLICATION_TASK_H
#define APPLICATION_TASK_H

#include <QString>
#include <QStringList>

#include <utils/network/http/qnbytearrayref.h>


namespace applauncher
{
    namespace api
    {
        namespace TaskType
        {
            enum Value
            {
                startApplication,
                quit,
                invalidTaskType
            };

            Value fromString( const QnByteArrayConstRef& str );
        }

        class BaseTask
        {
        public:
            const TaskType::Value type;

            BaseTask( TaskType::Value _type );
            virtual ~BaseTask();
        
            virtual QByteArray serialize() const = 0;
            virtual bool deserialize( const QByteArray& data ) = 0;
        };

        //!Parses \a serializedTask header, creates corresponding object (\a *ptr) and calls deserialize
        bool deserializeTask( const QByteArray& serializedTask, BaseTask** ptr );

        //!Task, sent by application, to start specified application version
        class StartApplicationTask
        :
            public BaseTask
        {
        public:
            //!Version in serialized format (e.g., 1.4.3)
            QString version;
            //!Command-line params to pass to application instance
            QString appArgs;

            StartApplicationTask();
            StartApplicationTask(
                const QString& _version,
                const QString& _appParams );
            StartApplicationTask(
                const QString& _version,
                const QStringList& _appParams );

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QByteArray& data ) override;
        };

        //!Applauncher process quits running on receiving this task
        class QuitTask
        :
            public BaseTask
        {
        public:
            QuitTask();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QByteArray& data ) override;
        };
    }
}

#endif  //APPLICATION_TASK_H
