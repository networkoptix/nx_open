////////////////////////////////////////////////////////////
// 15 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "applauncher_api.h"

#include <QtCore/QList>


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
                else if( str == "install" )
                    return install;
                else if( str == "getInstallationStatus" )
                    return getInstallationStatus;
                else
                    return invalidTaskType;
            }

            QByteArray toString( Value val )
            {
                switch( val )
                {
                    case startApplication:
                        return "run";
                    case quit:
                        return "quit";
                    case install:
                        return "install";
                    case getInstallationStatus:
                        return "getInstallationStatus";
                    default:
                        return "unknown";
                }
            }
        }


        BaseTask::BaseTask( TaskType::Value _type )
        :
            type( _type )
        {
        }

        BaseTask::~BaseTask()
        {
        }


        bool deserializeTask( const QByteArray& str, BaseTask** ptr )
        {
            const int taskNameEnd = str.indexOf('\n');
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

                case TaskType::install:
                    *ptr = new StartInstallationTask();
                    break;

                case TaskType::getInstallationStatus:
                    *ptr = new GetInstallationStatusRequest();
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
            return QString::fromLatin1("%1\n%2\n%3\n\n").arg(QLatin1String(TaskType::toString(type))).arg(version).arg(appArgs).toLatin1();
        }

        bool StartApplicationTask::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            version = QLatin1String(lines[1].toByteArrayWithRawData());
            appArgs = QLatin1String(lines[2].toByteArrayWithRawData());

            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class StartInstallationTask
        ////////////////////////////////////////////////////////////
        StartInstallationTask::StartInstallationTask()
        :
            BaseTask( TaskType::install ),
            module( QLatin1String("client") )
        {
        }

        //!Implementation of \a BaseTask::serialize()
        QByteArray StartInstallationTask::serialize() const
        {
            return QString::fromLatin1("%1\n%2\n%3\n\n").arg(QLatin1String(TaskType::toString(type))).arg(version).arg(module).toLatin1();
        }

        //!Implementation of \a BaseTask::deserialize()
        bool StartInstallationTask::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            version = QLatin1String(lines[1].toByteArrayWithRawData());
            module = QLatin1String(lines[2].toByteArrayWithRawData());
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
            return QString::fromLatin1("%1\n\n").arg(QLatin1String(TaskType::toString(type))).toLatin1();
        }

        bool QuitTask::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 1 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class Response
        ////////////////////////////////////////////////////////////
        namespace ResultType
        {
            Value fromString( const QnByteArrayConstRef& str )
            {
                if( str == "ok" )
                    return ok;
                else if( str == "versionNotInstalled" )
                    return versionNotInstalled;
                else if( str == "alreadyInstalled" )
                    return alreadyInstalled;
                else if( str == "invalidVersionFormat" )
                    return invalidVersionFormat;
                else if( str == "notFound" )
                    return notFound;
                else if( str == "ioError" )
                    return ioError;
                else
                    return otherError;
            }

            QByteArray toString( Value val )
            {
                switch( val )
                {
                    case ok:
                        return "ok";
                    case versionNotInstalled:
                        return "versionNotInstalled";
                    case alreadyInstalled:
                        return "alreadyInstalled";
                    case invalidVersionFormat:
                        return "invalidVersionFormat";
                    case notFound:
                        return "notFound";
                    case ioError:
                        return "ioError";
                    default:
                        return "otherError";
                }
            }
        }


        Response::Response()
        :
            result( ResultType::ok )
        {
        }

        QByteArray Response::serialize() const
        {
            return ResultType::toString(result) + "\n";
        }

        bool Response::deserialize( const QnByteArrayConstRef& data )
        {
            int sepPos = data.indexOf('\n');
            if( sepPos == -1 )
                return false;
            result = ResultType::fromString( data.mid(0, sepPos) );
            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class InstallResponse
        ////////////////////////////////////////////////////////////
        InstallResponse::InstallResponse()
        :
            installationID( 0 )
        {
        }

        QByteArray InstallResponse::serialize() const
        {
            return Response::serialize() + QByteArray::number(installationID) + "\n";
        }

        bool InstallResponse::deserialize( const QnByteArrayConstRef& data )
        {
            if( !Response::deserialize(data) )
                return false;
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 2 )
                return false;
            //line 0 - is a error code
            installationID = lines[1].toUInt();
            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class GetInstallationStatusRequest
        ////////////////////////////////////////////////////////////
        GetInstallationStatusRequest::GetInstallationStatusRequest()
        :
            BaseTask( TaskType::getInstallationStatus ),
            installationID( 0 )
        {
        }

        QByteArray GetInstallationStatusRequest::serialize() const
        {
            return QString::fromLatin1("%1\n%2\n\n").arg(QLatin1String(TaskType::toString(type))).arg(installationID).toLatin1();
        }

        bool GetInstallationStatusRequest::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 2 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            installationID = lines[1].toByteArrayWithRawData().toUInt();
            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class InstallationStatusResponse
        ////////////////////////////////////////////////////////////
        namespace InstallationStatus
        {
            Value fromString( const QnByteArrayConstRef& str )
            {
                if( str == "init" )
                    return init;
                else if( str == "inProgress" )
                    return inProgress;
                else if( str == "success" )
                    return success;
                else if( str == "failed" )
                    return failed;
                else
                    return unknown;
            }

            QByteArray toString( Value val )
            {
                switch( val )
                {
                    case init:
                        return "init";
                    case inProgress:
                        return "inProgress";
                    case success:
                        return "success";
                    case failed:
                        return "failed";
                    default:
                        return "unknown";
                }
            }
        }


        InstallationStatusResponse::InstallationStatusResponse()
        :
            status( InstallationStatus::init ),
            progress( 0 )
        {
        }

        QByteArray InstallationStatusResponse::serialize() const
        {
            return Response::serialize() + toString(status) + "\n" + QByteArray::number(progress) + "\n";
        }

        bool InstallationStatusResponse::deserialize( const QnByteArrayConstRef& data )
        {
            if( !Response::deserialize(data) )
                return false;

            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            //line 0 - is a error code
            status = InstallationStatus::fromString(lines[1]);
            progress = lines[1].toFloat();
            return true;
        }
    }
}
