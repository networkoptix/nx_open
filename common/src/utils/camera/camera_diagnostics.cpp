/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnostics.h"


namespace CameraDiagnostics
{
    namespace Step
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case none:
                    return QString::fromLatin1("none");
                case mediaServerAvailability:
                    return QString::fromLatin1("mediaServerAvailability");
                case cameraAvailability:
                    return QString::fromLatin1("cameraAvailability");
                case mediaStreamAvailability:
                    return QString::fromLatin1("mediaStreamAvailability");
                case mediaStreamIntegrity:
                    return QString::fromLatin1("mediaStreamIntegrity");
                default:
                    return QString::fromLatin1("unknown");
            }
        }

        Value fromString( const QString& str )
        {
            if( str == QString::fromLatin1("mediaServerAvailability") )
                return mediaServerAvailability;
            else if( str == QString::fromLatin1("cameraAvailability") )
                return cameraAvailability;
            else if( str == QString::fromLatin1("mediaStreamAvailability") )
                return mediaStreamAvailability;
            else if( str == QString::fromLatin1("mediaStreamIntegrity") )
                return mediaStreamIntegrity;
            else
                return none;
        }
    }

    namespace ErrorCode
    {
        //!Returns textual description of error with  parameters
        QString toString( Value val, const QList<QString>& errorParams )
        {
            int requiredParamCount = 0;
            QString errorMessage;
            switch( val )
            {
                case noError:
                    requiredParamCount = 0;
                    errorMessage = QObject::tr("ok");
                    break;
                case cannotEstablishConnection:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("Cannot connect to api port %1");
                    break;
                case cannotOpenCameraMediaPort:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("Cannot connect to media port %1");
                    break;
                case connectionClosedUnexpectedly:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("Connection to port %1 was closed unexpectedly");
                    break;
                case responseParseError:
                    errorMessage = QObject::tr("Error parsing server response");
                    break;
                case noMediaTrack:
                    errorMessage = QObject::tr("No media track(s)");
                    break;
                case notAuthorised:
                    errorMessage = QObject::tr("Not authorised");
                    break;
                case unsupportedProtocol:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("Unsupported media protocol %1");
                    break;
                case cannotConfigureMediaStream:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("Failed to configure parameter %1");
                    break;
                case requestFailed:
                    requiredParamCount = 2;
                    errorMessage = QObject::tr("Camera request %1 failed with error %2");
                    break;
                case notImplemented:
                    requiredParamCount = 0;
                    errorMessage = QObject::tr("Not implemented");
                    break;
                case ioError:
                    requiredParamCount = 1;
                    errorMessage = QObject::tr("I/O error. OS message: %1");
                    break;
                case serverTerminated:
                    errorMessage = QObject::tr("Server has been stopped");
                    break;
                default:
                    errorMessage = QObject::tr("Unknown error");
                    if( !errorParams.isEmpty() )
                        errorMessage += QObject::tr(". Parameters: ");
                    for( int i = 0; i < errorParams.size(); ++i )
                    {
                        if( i > 0 )
                            errorMessage += QLatin1String(", ");
                        errorMessage += errorParams[i];
                    }
                    break;
            }

            requiredParamCount = std::min<int>(requiredParamCount, errorParams.size());
            for( int i = 0; i < requiredParamCount; ++i )
                errorMessage = errorMessage.arg(errorParams[i]);

            return errorMessage;
        }

        QString toString( int val, const QList<QString>& errorParams )
        {
            return toString( static_cast<Value>(val), errorParams );
        }
    }


    Result::Result()
    :
        errorCode( ErrorCode::noError )
    {
    }

    Result::Result( ErrorCode::Value _errorCode, const QString& param1, const QString& param2 )
    :
        errorCode( _errorCode )
    {
        if( !param1.isEmpty() || !param2.isEmpty() )
            errorParams.push_back( param1 );
        if( !param2.isEmpty() )
            errorParams.push_back( param2 );
    }

    QString Result::toString() const
    {
        return ErrorCode::toString( errorCode, errorParams );
    }

    Result::operator bool() const
    {
        return errorCode == ErrorCode::noError;
    }
}
