/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnostics.h"

#include <QtCore/QCoreApplication>


class QnCameraDiagnosticsErrorCodeStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraDiagnosticsErrorCodeStrings);

public:
    //!Returns textual description of error with  parameters
    static QString toString( CameraDiagnostics::ErrorCode::Value val, const QList<QString>& errorParams )
    {
        using namespace CameraDiagnostics::ErrorCode;

        int requiredParamCount = 0;
        QStringList errorMessageParts;
        switch( val )
        {
            case noError:
                requiredParamCount = 0;
                errorMessageParts << tr("OK");
                break;
            case mediaServerUnavailable:
                requiredParamCount = 1;
                errorMessageParts   << tr("Server %1 is not available.")
                                    << tr("Check that Server is up and running.");
                break;
            case mediaServerBadResponse:
                requiredParamCount = 2;
                errorMessageParts   << tr("Received bad response from Server %1: \"%2\".") 
                                    << tr("Check if Server is up and has the proper version.");
                break;
            case cannotEstablishConnection:
                requiredParamCount = 1;
                errorMessageParts   << tr("Cannot connect to http port %1.") 
                                    << tr("Make sure the camera is plugged into the network.");
                break;
            case cannotOpenCameraMediaPort:
                requiredParamCount = 2;
                errorMessageParts   << tr("Cannot open media url %1. Failed to connect to media port %2.")
                                    << tr("Make sure port %2 is accessible (e.g. forwarded). Please try to reboot the camera, then restore factory defaults on the web-page.");
                break;
            case connectionClosedUnexpectedly:
                requiredParamCount = 2;
                errorMessageParts   << tr("Cannot open media url %1. Connection to port %2 was closed unexpectedly.")
                                    << tr("Make sure the camera is plugged into the network. Try to reboot the camera.");
                break;
            case responseParseError:
                requiredParamCount = 2;
                errorMessageParts   << tr("Could not parse camera response. Url %1, request name %2.")
                                    << tr("Please try to reboot the camera, then restore factory defaults on the web-page.")
                                    << tr("Finally, try to update firmware. If the problem persists, please contact support.");
                break;
            case noMediaTrack:
                requiredParamCount = 1;
                errorMessageParts   << tr("No supported media tracks at url %1.")
                                    << tr("Please try to reboot the camera, then restore factory defaults on the web-page.")
                                    << tr("Finally, try to update firmware. If the problem persists, please contact support.");
                break;
            case notAuthorised:
                requiredParamCount = 1;
                errorMessageParts   << tr("Not authorized. Url %1.");
                break;
            case unsupportedProtocol:
                requiredParamCount = 2;
                errorMessageParts   << tr("Cannot open media url %1. Unsupported media protocol %2.")
                                    << tr("Please try to reboot the camera, then restore factory defaults on the web-page.")
                                    << tr("Finally, try to update firmware. If the problem persists, please contact support.");
                break;
            case cannotConfigureMediaStream:
                requiredParamCount = 1;
                errorMessageParts   << tr("Failed to configure parameter %1.")
                                    << tr("First, try to turn on recording (if it's off) and decrease fps in camera settings.")
                                    << tr("If it doesn't help, restore factory defaults on the camera web-page. If the problem persists, please contact support.");
                break;
            case requestFailed:
                requiredParamCount = 2;
                errorMessageParts   << tr("Camera request \"%1\" failed with error \"%2\".")
                                    << tr("Please try to reboot the camera, then restore factory defaults on the web-page.")
                                    << tr("Finally, try to update firmware. If the problem persists, please contact support.");
                break;
            case notImplemented:
                requiredParamCount = 0;
                errorMessageParts   << tr("Unknown Camera Issue.")
                                    << tr("Please contact support.");
                break;
            case ioError:
                requiredParamCount = 1;
                errorMessageParts   << tr("An input/output error has occurred. OS message: \"%1\".")
                                    << tr("Make sure the camera is plugged into the network. Try to reboot the camera.");
                break;
            case serverTerminated:
                errorMessageParts   << tr("Server has been stopped.");
                break;
            case cameraInvalidParams:
                requiredParamCount = 1;
                errorMessageParts   << tr("Invalid data was received from the camera: %1.");
                break;
            case badMediaStream:
                errorMessageParts   << tr("Too many media errors. Please open camera issues dialog for more details.");
                break;
            case noMediaStream:
                errorMessageParts   << tr("Media stream is opened but no media data was received.");
                break;
            case cameraInitializationInProgress:
                errorMessageParts   << tr("Camera initialization process in progress");
                break;
            case cameraPluginError:
                requiredParamCount = 1;
                errorMessageParts   << tr("Camera plugin error. %1");
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

                errorMessageParts << tr("Unknown error. Please contact support.");
                if( nonEmptyParamCount ) {
                    QString params = tr("Parameters:");
                    
                    for( int i = 0; i < nonEmptyParamCount; ++i )
                    {
                        if( i > 0 )
                            params += QLatin1String(", ");
                        params += errorParams[i];
                    }
                    errorMessageParts << QString() << QString() << params;
                }
                break;
            }
        }

        QString errorMessage = errorMessageParts.join(L'\n');
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
        // TODO : #Elric classic enum name mapping

        QString toString( Value val )
        {
            switch( val )
            {
                case none:
                    return lit("none");
                case mediaServerAvailability:
                    return lit("mediaServerAvailability");
                case cameraAvailability:
                    return lit("cameraAvailability");
                case mediaStreamAvailability:
                    return lit("mediaStreamAvailability");
                case mediaStreamIntegrity:
                    return lit("mediaStreamIntegrity");
                default:
                    return lit("unknown");
            }
        }

        Value fromString( const QString& str )
        {
            if( str == lit("mediaServerAvailability") )
                return mediaServerAvailability;
            else if( str == lit("cameraAvailability") )
                return cameraAvailability;
            else if( str == lit("mediaStreamAvailability") )
                return mediaStreamAvailability;
            else if( str == lit("mediaStreamIntegrity") )
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
