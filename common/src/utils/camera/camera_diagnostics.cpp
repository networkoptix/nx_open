/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnostics.h"

#include <QtCore/QCoreApplication>

#include <core/resource/device_dependent_strings.h>

class QnCameraDiagnosticsErrorCodeStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraDiagnosticsErrorCodeStrings);

public:
    //!Returns textual description of error with  parameters
    static QString toString( CameraDiagnostics::ErrorCode::Value val
        , const QnVirtualCameraResourcePtr &device
        , const QList<QString>& errorParams )
    {
        using namespace CameraDiagnostics::ErrorCode;

        QnCameraDeviceStringSet detailsRebootRestore(
            tr("Please try to reboot the device, then restore factory defaults on the web-page."),
            tr("Please try to reboot the camera, then restore factory defaults on the web-page."),
            tr("Please try to reboot the I/O module, then restore factory defaults on the web-page.")
            );

        QnCameraDeviceStringSet detailsPluggedReboot(
            tr("Make sure the device is plugged into the network. Try to reboot the device."),
            tr("Make sure the camera is plugged into the network. Try to reboot the camera."),
            tr("Make sure the I/O module is plugged into the network. Try to reboot the I/O module.")
            );

        QString firmwareSupport = tr("Finally, try to update firmware. If the problem persists, please contact support.");


        int requiredParamCount = 0;
        QStringList errorMessageParts;
        switch( val )
        {
            case noError:
                {
                    requiredParamCount = 0;
                    errorMessageParts
                        << tr("OK");
                    break;
                }
            case mediaServerUnavailable:
                {
                    requiredParamCount = 1;
                    errorMessageParts
                        << tr("Server %1 is not available.")
                        << tr("Check that Server is up and running.");
                    break;
                }
            case mediaServerBadResponse:
                {
                    requiredParamCount = 2;
                    errorMessageParts
                        << tr("Received bad response from Server %1: \"%2\".")
                        << tr("Check if Server is up and has the proper version.");
                    break;
                }
            case cannotEstablishConnection:
                {
                    requiredParamCount = 1;
                    errorMessageParts
                        << tr("Cannot connect to http port %1.")
                        << QnDeviceDependentStrings::getNameFromSet(detailsPluggedReboot, device);
                    break;
                }
            case cannotOpenCameraMediaPort:
                {
                    requiredParamCount = 2;
                    errorMessageParts   << tr("Cannot open media url %1. Failed to connect to media port %2.")
                        << tr("Make sure port %2 is accessible (e.g. forwarded).")
                        << QnDeviceDependentStrings::getNameFromSet(detailsRebootRestore, device);
                    break;
                }
            case connectionClosedUnexpectedly:
                {
                    requiredParamCount = 2;
                    errorMessageParts
                        << tr("Cannot open media url %1. Connection to port %2 was closed unexpectedly.")
                        << QnDeviceDependentStrings::getNameFromSet(detailsPluggedReboot, device);
                    break;
                }
            case responseParseError:
                {
                    requiredParamCount = 2;
                    QnCameraDeviceStringSet detailsBase(
                        tr("Could not parse device response. Url %1, request name %2."),
                        tr("Could not parse camera response. Url %1, request name %2."),
                        tr("Could not parse I/O module response. Url %1, request name %2.")
                        );

                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(detailsBase, device)
                        << QnDeviceDependentStrings::getNameFromSet(detailsRebootRestore, device)
                        << firmwareSupport;
                    break;
                }
            case noMediaTrack:
                {
                    requiredParamCount = 1;
                    errorMessageParts   << tr("No supported media tracks at url %1.")
                        << QnDeviceDependentStrings::getNameFromSet(detailsRebootRestore, device)
                        << firmwareSupport;
                    break;
                }
            case notAuthorised:
                {
                    requiredParamCount = 1;
                    errorMessageParts   << tr("Not authorized. Url %1.");
                    break;
                }
            case unsupportedProtocol:
                {
                    requiredParamCount = 2;
                    errorMessageParts
                        << tr("Cannot open media url %1. Unsupported media protocol %2.")
                        << QnDeviceDependentStrings::getNameFromSet(detailsRebootRestore, device)
                        << firmwareSupport;
                    break;
                }
            case cannotConfigureMediaStream:
                {
                    requiredParamCount = 1;
                    QnCameraDeviceStringSet detailsBase(
                        tr("First, try to turn on recording (if it's off) and decrease fps in device settings."),
                        tr("First, try to turn on recording (if it's off) and decrease fps in camera settings."),
                        tr("First, try to turn on recording (if it's off) and decrease fps in I/O module settings.")
                        );
                    QnCameraDeviceStringSet detailsAdvanced(
                        tr("If it doesn't help, restore factory defaults on the device web-page."),
                        tr("If it doesn't help, restore factory defaults on the camera web-page."),
                        tr("If it doesn't help, restore factory defaults on the I/O module web-page.")
                        );

                    errorMessageParts
                        << tr("Failed to configure parameter %1.")
                        << QnDeviceDependentStrings::getNameFromSet(detailsBase, device)
                        << QnDeviceDependentStrings::getNameFromSet(detailsAdvanced, device)
                        << firmwareSupport;
                    break;
                }
            case requestFailed:
                {
                    requiredParamCount = 2;
                    QnCameraDeviceStringSet detailsBase(
                        tr("Device request \"%1\" failed with error \"%2\"."),
                        tr("Camera request \"%1\" failed with error \"%2\"."),
                        tr("I/O Module request \"%1\" failed with error \"%2\".")
                        );
                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(detailsBase, device)
                        << QnDeviceDependentStrings::getNameFromSet(detailsRebootRestore, device)
                        << firmwareSupport;
                    break;
                }
            case notImplemented:
                {
                    requiredParamCount = 0;
                    QnCameraDeviceStringSet details(
                        tr("Unknown device issue."),
                        tr("Unknown camera issue."),
                        tr("Unknown I/O module issue.")
                        );

                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(details, device)
                        << tr("Please contact support.");
                    break;
                }
            case ioError:
                {
                    requiredParamCount = 1;
                    errorMessageParts
                        << tr("An input/output error has occurred. OS message: \"%1\".")
                        << QnDeviceDependentStrings::getNameFromSet(detailsPluggedReboot, device);
                    break;
                }
            case serverTerminated:
                {
                    requiredParamCount = 0;
                    errorMessageParts   << tr("Server has been stopped.");
                    break;
                }
            case cameraInvalidParams:
                {
                    requiredParamCount = 1;
                    QnCameraDeviceStringSet details(
                        tr("Invalid data was received from the device %1."),
                        tr("Invalid data was received from the camera %1."),
                        tr("Invalid data was received from the I/O module %1.")
                        );

                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(details, device);
                    break;
                }
            case badMediaStream:
                {
                    requiredParamCount = 0;
                    QnCameraDeviceStringSet details(
                        tr("Too many media errors. Please open device issues dialog for more details."),
                        tr("Too many media errors. Please open camera issues dialog for more details."),
                        tr("Too many media errors. Please open I/O module issues dialog for more details.")
                        );

                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(details, device);
                    break;
                }
            case noMediaStream:
                {
                    requiredParamCount = 0;
                    errorMessageParts
                        << tr("Media stream is opened but no media data was received.");
                    break;
                }
            case cameraInitializationInProgress:
                {
                    requiredParamCount = 0;
                    QnCameraDeviceStringSet details(
                        tr("Device initialization process is in progress."),
                        tr("Camera initialization process is in progress."),
                        tr("I/O Module initialization process is in progress.")
                        );

                    errorMessageParts
                        << QnDeviceDependentStrings::getNameFromSet(details, device);
                    break;
				}
            case cameraPluginError:
				{
	                requiredParamCount = 1;
    	            errorMessageParts   << tr("Camera plugin error. %1");
					break;
				}
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

        QString toString(Value val
            , const QnVirtualCameraResourcePtr &device
            , const ErrorParams& errorParams)
        {
            return QnCameraDiagnosticsErrorCodeStrings::toString(val, device, errorParams);
        }

        QString toString(int val
            , const QnVirtualCameraResourcePtr &device
            , const ErrorParams& errorParams)
        {
            return toString(static_cast<Value>(val), device, errorParams);
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
        return ErrorCode::toString( errorCode, QnVirtualCameraResourcePtr(), errorParams );
    }

    Result::operator safe_bool_type() const
    {
        return errorCode == ErrorCode::noError ? &Result::safe_bool_type_retval : 0;
    }
}
