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
                else if( str == "installZip" )
                    return installZip;
                else if( str == "getInstallationStatus" )
                    return getInstallationStatus;
                else if( str == "isVersionInstalled" )
                    return isVersionInstalled;
                else if( str == "getInstalledVersions" )
                    return getInstalledVersions;
                else if( str == "cancelInstallation" )
                    return cancelInstallation;
                else if( str == "addProcessKillTimer" )
                    return addProcessKillTimer;
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
                    case installZip:
                        return "installZip";
                    case getInstallationStatus:
                        return "getInstallationStatus";
                    case isVersionInstalled:
                        return "isVersionInstalled";
                    case getInstalledVersions:
                        return "getInstalledVersions";
                    case cancelInstallation:
                        return "cancelInstallation";
                    case addProcessKillTimer:
                        return "addProcessKillTimer";
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

                case TaskType::installZip:
                    *ptr = new InstallZipTask();
                    break;

                case TaskType::isVersionInstalled:
                    *ptr = new IsVersionInstalledRequest();
                    break;
                    
                case TaskType::getInstalledVersions:
                    *ptr = new GetInstalledVersionsRequest();
                    break;

                case TaskType::cancelInstallation:
                    *ptr = new CancelInstallationRequest();
                    break;

                case TaskType::addProcessKillTimer:
                    *ptr = new AddProcessKillTimerRequest();
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
            BaseTask( TaskType::startApplication ),
            autoRestore( false )
        {
        }

        StartApplicationTask::StartApplicationTask(
            const QnSoftwareVersion &_version,
            const QString& _appParams )
        :
            BaseTask( TaskType::startApplication ),
            version( _version ),
            appArgs( _appParams ),
            autoRestore( false )
        {
        }

        StartApplicationTask::StartApplicationTask(
            const QnSoftwareVersion &_version,
            const QStringList& _appParams )
        :
            BaseTask( TaskType::startApplication ),
            version( _version ),
            appArgs( _appParams.join(QLatin1String(" ")) ),
            autoRestore( false )
        {
        }

        QByteArray StartApplicationTask::serialize() const
        {
            return lit("%1\n%2\n%3\n%4\n\n").arg(QLatin1String(TaskType::toString(type))).
                arg(version.toString()).arg(appArgs).
                arg(autoRestore ? QLatin1String("true") : QLatin1String("false")).toLatin1();
        }

        bool StartApplicationTask::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            version = QnSoftwareVersion(lines[1].toByteArrayWithRawData());
            appArgs = QLatin1String(lines[2].toByteArrayWithRawData());
            if( lines.size() > 3 )
                autoRestore = lines[3].toByteArrayWithRawData() == "true";

            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class StartInstallationTask
        ////////////////////////////////////////////////////////////
        static const QLatin1String DEFAULT_MODULE_NAME( "client" );

        StartInstallationTask::StartInstallationTask()
        :
            BaseTask( TaskType::install ),
            module( DEFAULT_MODULE_NAME ),
            autoStart( false )
        {
        }

        StartInstallationTask::StartInstallationTask(const QnSoftwareVersion &_version, bool _autoStart )
        :
            BaseTask( TaskType::install ),
            version( _version ),
            module( DEFAULT_MODULE_NAME ),
            autoStart( _autoStart )
        {
        }

        //!Implementation of \a BaseTask::serialize()
        QByteArray StartInstallationTask::serialize() const
        {
            return lit("%1\n%2\n%3\n\n").arg(QLatin1String(TaskType::toString(type))).arg(version.toString()).arg(module).toLatin1();
        }

        //!Implementation of \a BaseTask::deserialize()
        bool StartInstallationTask::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            version = QnSoftwareVersion(lines[1].toByteArrayWithRawData());
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
            return lit("%1\n\n").arg(QLatin1String(TaskType::toString(type))).toLatin1();
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
        IsVersionInstalledRequest::IsVersionInstalledRequest()
        :
            BaseTask( TaskType::isVersionInstalled )
        {
        }

        QByteArray IsVersionInstalledRequest::serialize() const
        {
            return lit("%1\n%2\n\n").arg(QLatin1String(TaskType::toString(type))).arg(version.toString()).toLatin1();
        }
        
        bool IsVersionInstalledRequest::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 2 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            version = QnSoftwareVersion(lines[1].toByteArrayWithRawData());
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
                else if( str == "connectError" )
                    return connectError;
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
                    case connectError:
                        return "connectError";
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
        //// class StartInstallationResponse
        ////////////////////////////////////////////////////////////
        StartInstallationResponse::StartInstallationResponse()
        :
            installationID( 0 )
        {
        }

        QByteArray StartInstallationResponse::serialize() const
        {
            return Response::serialize() + QByteArray::number(installationID) + "\n";
        }

        bool StartInstallationResponse::deserialize( const QnByteArrayConstRef& data )
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

        GetInstallationStatusRequest::GetInstallationStatusRequest( unsigned int _installationID )
        :
            BaseTask( TaskType::getInstallationStatus ),
            installationID( _installationID )
        {
        }

        QByteArray GetInstallationStatusRequest::serialize() const
        {
            return lit("%1\n%2\n\n").arg(QLatin1String(TaskType::toString(type))).arg(installationID).toLatin1();
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
                else if( str == "cancelled" )
                    return cancelled;
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
                    case cancelled:
                        return "cancelled";
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
            progress = lines[2].toFloat();
            return true;
        }


        ////////////////////////////////////////////////////////////
        //// class InstallationDirRequest
        ////////////////////////////////////////////////////////////
        InstallZipTask::InstallZipTask()
        :
            BaseTask ( TaskType::installZip )
        {
        }

        InstallZipTask::InstallZipTask(
            const QnSoftwareVersion &version,
            const QString &zipFileName)
        :
            BaseTask ( TaskType::installZip ),
            version(version),
            zipFileName(zipFileName)
        {
        }

        QByteArray InstallZipTask::serialize() const
        {
            return lit("%1\n%2\n%3\n\n")
                    .arg(QString::fromLatin1(TaskType::toString(type)))
                    .arg(version.toString())
                    .arg(zipFileName).toUtf8();
        }

        bool InstallZipTask::deserialize(const QnByteArrayConstRef &data)
        {
            QStringList list = QString::fromUtf8(data).split(lit("\n"), QString::SkipEmptyParts);
            if (list.size() < 3)
                return false;
            if (TaskType::toString(type) != list[0].toLatin1())
                return false;
            version = QnSoftwareVersion(list[1]);
            zipFileName = list[2];
            return true;
        }

        ////////////////////////////////////////////////////////////
        //// class IsVersionInstalledResponse
        ////////////////////////////////////////////////////////////
        IsVersionInstalledResponse::IsVersionInstalledResponse()
        :
            installed( false )
        {
        }

        QByteArray IsVersionInstalledResponse::serialize() const
        {
            return Response::serialize() + QByteArray::number(installed ? 1 : 0) + "\n\n";
        }
        
        bool IsVersionInstalledResponse::deserialize( const QnByteArrayConstRef& data )
        {
            if( !Response::deserialize(data) )
                return false;

            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 2 )
                return false;
            //line 0 - is a error code
            installed = lines[1].toUInt() > 0;
            return true;
        }



        ////////////////////////////////////////////////////////////
        //// class CancelInstallationRequest
        ////////////////////////////////////////////////////////////
        CancelInstallationRequest::CancelInstallationRequest()
        :
            BaseTask( TaskType::cancelInstallation ),
            installationID( 0 )
        {
        }

        QByteArray CancelInstallationRequest::serialize() const
        {
            return lit("%1\n%2\n\n").arg(QLatin1String(TaskType::toString(type))).arg(installationID).toLatin1();
        }

        bool CancelInstallationRequest::deserialize( const QnByteArrayConstRef& data )
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
        //// class AddProcessKillTimerRequest
        ////////////////////////////////////////////////////////////
        AddProcessKillTimerRequest::AddProcessKillTimerRequest()
        :
            BaseTask( TaskType::addProcessKillTimer ),
            processID( 0 )
        {
        }

        QByteArray AddProcessKillTimerRequest::serialize() const
        {
            return lit("%1\n%2\n%3\n\n").arg(QLatin1String(TaskType::toString(type))).arg(processID).arg(timeoutMillis).toLatin1();
        }

        bool AddProcessKillTimerRequest::deserialize( const QnByteArrayConstRef& data )
        {
            const QList<QnByteArrayConstRef>& lines = data.split('\n');
            if( lines.size() < 3 )
                return false;
            if( lines[0] != TaskType::toString(type) )
                return false;
            processID = lines[1].toByteArrayWithRawData().toUInt();
            timeoutMillis = lines[2].toByteArrayWithRawData().toUInt();
            return true;
        }

        ////////////////////////////////////////////////////////////
        //// class GetInstalledVersionsRequest
        ////////////////////////////////////////////////////////////
        GetInstalledVersionsRequest::GetInstalledVersionsRequest()
        :
             BaseTask( TaskType::getInstalledVersions )
        {
        }

        QByteArray GetInstalledVersionsRequest::serialize() const
        {
            return TaskType::toString(type) + "\n";
        }

        bool GetInstalledVersionsRequest::deserialize(const QnByteArrayConstRef &data)
        {
            return data == serialize();
        }

        ////////////////////////////////////////////////////////////
        //// class GetInstalledVersionsResponse
        ////////////////////////////////////////////////////////////
        GetInstalledVersionsResponse::GetInstalledVersionsResponse()
        {
        }

        QByteArray GetInstalledVersionsResponse::serialize() const
        {
            QByteArray result = Response::serialize();
            foreach (const QnSoftwareVersion &version, versions) {
                result.append(version.toString().toLatin1());
                result.append(',');
            }
            result[result.size() - 1] = '\n';
            return result;
        }

        bool GetInstalledVersionsResponse::deserialize(const QnByteArrayConstRef &data)
        {
            if (!Response::deserialize(data))
                return false;

            const QList<QnByteArrayConstRef> &lines = data.split('\n');
            if (lines.size() < 2)
                return false;

            const QList<QnByteArrayConstRef> &versions = lines[1].split(',');
            foreach (const QnByteArrayConstRef &version, versions)
                this->versions.append(QnSoftwareVersion(version.toByteArrayWithRawData()));

            return true;
        }
    }
}
