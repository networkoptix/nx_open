////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef APPLICATION_TASK_H
#define APPLICATION_TASK_H

#include <QtCore/QString>
#include <QtCore/QStringList>

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
                install,
                getInstallationStatus,
                isVersionInstalled,
                cancelInstallation,
                addProcessKillTimer,
                invalidTaskType
            };

            Value fromString( const QnByteArrayConstRef& str );
            QByteArray toString( Value val );
        }

        class BaseTask
        {
        public:
            const TaskType::Value type;

            BaseTask( TaskType::Value _type );
            virtual ~BaseTask();
        
            virtual QByteArray serialize() const = 0;
            virtual bool deserialize( const QnByteArrayConstRef& data ) = 0;
        };

        //!Parses \a serializedTask header, creates corresponding object (\a *ptr) and calls deserialize
        bool deserializeTask( const QByteArray& serializedTask, BaseTask** ptr );

        //!Task, sent by application, to start specified application version
        class StartApplicationTask
        :
            public BaseTask
        {
        public:
            //!Version in format 1.4.3
            QString version;
            //!Command-line params to pass to application instance
            QString appArgs;
            bool autoRestore;

            StartApplicationTask();
            StartApplicationTask(
                const QString& _version,
                const QString& _appParams );
            StartApplicationTask(
                const QString& _version,
                const QStringList& _appParams );

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class StartInstallationTask
        :
            public BaseTask
        {
        public:
            //!Version in format 1.4
            QString version;
            //!Module name for install. By default, "client". For future use
            QString module;
            bool autoStart;

            StartInstallationTask();
            StartInstallationTask( const QString& _version, bool _autoStart = false );

            //!Implementation of \a BaseTask::serialize()
            virtual QByteArray serialize() const override;
            //!Implementation of \a BaseTask::deserialize()
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        //!Applauncher process quits running on receiving this task
        class QuitTask
        :
            public BaseTask
        {
        public:
            QuitTask();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class IsVersionInstalledRequest
        :
            public BaseTask
        {
        public:
            //!Version in format 1.4
            QString version;

            IsVersionInstalledRequest();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        namespace ResultType
        {
            enum Value
            {
                ok,
                //!Failed to connect to applauncher
                connectError,
                versionNotInstalled,
                alreadyInstalled,
                invalidVersionFormat,
                notFound,
                badResponse,
                ioError,
                otherError
            };
            Value fromString( const QnByteArrayConstRef& str );
            QByteArray toString( Value val );
        }

        class Response
        {
        public:
            ResultType::Value result;

            Response();

            virtual QByteArray serialize() const;
            virtual bool deserialize( const QnByteArrayConstRef& data );
        };

        class StartInstallationResponse
        :
            public Response
        {
        public:
            //!Valid if \a result == \a ResultType::ok
            unsigned int installationID;

            StartInstallationResponse();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class GetInstallationStatusRequest
        :
            public BaseTask
        {
        public:
            unsigned int installationID;

            GetInstallationStatusRequest();
            GetInstallationStatusRequest( unsigned int installationID );

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        namespace InstallationStatus
        {
            enum Value
            {
                init,
                inProgress,
                //!installation is being cancelled (removing already installed files)
                cancelInProgress,
                success,
                failed,
                cancelled,
                unknown
            };

            Value fromString( const QnByteArrayConstRef& str );
            QByteArray toString( Value val );
        }

        class InstallationStatusResponse
        :
            public Response
        {
        public:
            InstallationStatus::Value status;
            //!Installation progress (percent)
            float progress;

            InstallationStatusResponse();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class IsVersionInstalledResponse
        :
            public Response
        {
        public:
            bool installed;

            IsVersionInstalledResponse();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class CancelInstallationRequest
        :
            public BaseTask
        {
        public:
            unsigned int installationID;

            CancelInstallationRequest();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class CancelInstallationResponse : public Response {};


        class AddProcessKillTimerRequest
        :
            public BaseTask
        {
        public:
            qint64 processID;
            quint32 timeoutMillis;

            AddProcessKillTimerRequest();

            virtual QByteArray serialize() const override;
            virtual bool deserialize( const QnByteArrayConstRef& data ) override;
        };

        class AddProcessKillTimerResponse : public Response {};
    }
}

#endif  //APPLICATION_TASK_H
