/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnostics.h"


class QnCameraDiagnosticsErrorCodeStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraDiagnosticsErrorCodeStrings);

public:
    //!Returns textual description of error with  parameters
    static QString toString( CameraDiagnostics::ErrorCode::Value val, const QList<QString>& errorParams )
    {
        using namespace CameraDiagnostics::ErrorCode;

        int requiredParamCount = 0;
        QString errorMessage;
        switch( val )
        {
            case noError:
                requiredParamCount = 0;
                errorMessage = tr("ok");
                break;
            case mediaServerUnavailable:
                requiredParamCount = 1;
                errorMessage = tr("Media server %1 is not available.\n Check that media server is up and running.");
                break;
            case mediaServerBadResponse:
                requiredParamCount = 2;
                errorMessage = tr("Received bad response from media server %1: \"%2\".\n Check if Media Server is up and has the proper version.");
                break;
            case cannotEstablishConnection:
                requiredParamCount = 1;
                errorMessage = tr("Cannot connect to http port %1.\n Make sure the camera is plugged into the network.");
                break;
            case cannotOpenCameraMediaPort:
                requiredParamCount = 2;
                errorMessage = tr("Cannot open media url %1. Failed to connect to media port %2.\n "
                    "Make sure port %2 is accessible (forwarded etc). Please try to reboot the camera, then restore factory defaults on the web-page.");
                break;
            case connectionClosedUnexpectedly:
                requiredParamCount = 2;
                errorMessage = tr("Cannot open media url %1. Connection to port %2 was closed unexpectedly.\n "
                                  "Make sure the camera is plugged into the network. Try to reboot camera.");
                break;
            case responseParseError:
                requiredParamCount = 2;
                errorMessage = tr("Error parsing camera response. Url %1, request name %2.\n Please try to reboot the camera, "
                    "then restore factory defaults on the web-page. Finally, try to update firmware. If the problem persists, contact support");
                break;
            case noMediaTrack:
                requiredParamCount = 1;
                errorMessage = tr("No supported media track(s) at url %1.\n Please try to reboot the camera, then restore factory defaults on the web-page. "
                    "Finally, try to update firmware. If the problem persists, contact support");
                break;
            case notAuthorised:
                requiredParamCount = 1;
                errorMessage = tr("Not authorized. Url %1");
                break;
            case unsupportedProtocol:
                requiredParamCount = 2;
                errorMessage = tr("Cannot open media url %1. Unsupported media protocol %2.\n Please try to restore factory defaults on the web-page. "
                    "Finally, try to update firmware. If the problem persists, contact support.");
                break;
            case cannotConfigureMediaStream:
                requiredParamCount = 1;
                errorMessage = tr("Failed to configure parameter %1.\n First, try to turn on recording (if it's off) and decrease fps in camera settings. "
                    "If no success, restore factory defaults on the camera web-page. If the problem persists, contact support.");
                break;
            case requestFailed:
                requiredParamCount = 2;
                errorMessage = tr("Camera request \"%1\" failed with error \"%2\".\n Please try to reboot the camera, then restore factory defaults on the web-page. "
                    "Finally, try to update firmware. If the problem persists, contact support.");
                break;
            case notImplemented:
                requiredParamCount = 0;
                errorMessage = tr("Unknown Camera Issue.\n Please, contact support.");
                break;
            case ioError:
                requiredParamCount = 1;
                errorMessage = tr("I/O error. OS message: \"%1\".\n Make sure the camera is plugged into the network. Try to reboot the camera.");
                break;
            case serverTerminated:
                errorMessage = tr("Server has been stopped.");
                break;
            case cameraInvalidParams:
                requiredParamCount = 1;
                errorMessage = tr("Invalid data received from camera: %1");
                break;
            case badMediaStream:
                errorMessage = tr("Too many media errors. Please open camera issues dialog for more details");
                break;
            default:
            {
                int nonEmptyParamCount = 0;
                for( int i = 0; i < errorParams.size(); ++i )
                {
                    if( !errorParams[i].isEmpty() )
                        ++nonEmptyParamCount;
                    else
                        break;
                }

                errorMessage = tr("Unknown error. Please contact support.");
                if( nonEmptyParamCount )
                    errorMessage += tr(" Parameters: ");
                for( int i = 0; i < nonEmptyParamCount; ++i )
                {
                    if( i > 0 )
                        errorMessage += QLatin1String(", ");
                    errorMessage += errorParams[i];
                }
                break;
            }
        }

        requiredParamCount = std::min<int>(requiredParamCount, errorParams.size());
        for( int i = 0; i < requiredParamCount; ++i )
            errorMessage = errorMessage.arg(!errorParams[i].isEmpty() ? errorParams[i] : tr("(unknown)"));

        return errorMessage;
    }
};


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
            return QnCameraDiagnosticsErrorCodeStrings::toString( val, errorParams );
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
        errorParams.push_back(param1);
        errorParams.push_back(param2);
    }

    QString Result::toString() const
    {
        return ErrorCode::toString( errorCode, errorParams );
    }

    Result::operator safe_bool_type() const
    {
        return errorCode == ErrorCode::noError ? &Result::safe_bool_type_retval : 0;
    }
}
