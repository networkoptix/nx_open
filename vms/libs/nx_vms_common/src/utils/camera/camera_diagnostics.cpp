// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_diagnostics.h"

#include <QtCore/QCoreApplication>

#include <core/resource/device_dependent_strings.h>

#include <nx/utils/log/assert.h>
#include <core/resource/camera_resource.h>

class QnCameraDiagnosticsErrorCodeStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraDiagnosticsErrorCodeStrings)

public:
    //!Returns textual description of error with  parameters
    static QString toString(
        CameraDiagnostics::ErrorCode::Value val,
        QnResourcePool* resourcePool,
        const QnVirtualCameraResourcePtr& device,
        const QList<QString>& errorParams)
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

        auto getParam = [&errorParams](int index)
        {
            QString result;
            if (errorParams.size() > index)
                result = errorParams[index];

            return result.isEmpty()
                ? tr("(unknown)")
                : result;
        };

        // Pre-cache to simplify format strings
        const QString p1 = getParam(0);
        const QString p2 = getParam(1);

        QStringList errorMessageParts;
        switch (val)
        {
            case noError:
            {
                errorMessageParts
                    << tr("OK");
                break;
            }
            case mediaServerUnavailable:
            {
                errorMessageParts
                    << tr("Server %1 is not available.").arg(p1)
                    << tr("Check that Server is up and running.");
                break;
            }
            case mediaServerBadResponse:
            {
                errorMessageParts
                    << tr("Received bad response from Server %1: \"%2\".").arg(p1).arg(p2)
                    << tr("Check if Server is up and has the proper version.");
                break;
            }
            case cannotEstablishConnection:
            {
                errorMessageParts
                    << tr("Cannot connect to http port %1.").arg(p1)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsPluggedReboot, device);
                break;
            }
            case cannotOpenCameraMediaPort:
            {
                errorMessageParts
                    << tr("Cannot open media URL %1. Failed to connect to media port %2.").arg(p1).arg(p2)
                    << tr("Make sure port %1 is accessible (e.g. forwarded).").arg(p2)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsRebootRestore, device);
                break;
            }
            case liveVideoIsNotSupportedError:
            {
                errorMessageParts
                    << tr("Camera was restored from archive. Delete the camera and add it again to view Live video.");
                break;
            }
            case connectionClosedUnexpectedly:
            {
                errorMessageParts
                    << tr("Cannot open media URL %1. Connection to port %2 was closed unexpectedly.").arg(p1).arg(p2)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsPluggedReboot, device);
                break;
            }
            case responseParseError:
            {
                QnCameraDeviceStringSet detailsBase(
                    tr("Could not parse device response. URL %1, request name %2.").arg(p1).arg(p2),
                    tr("Could not parse camera response. URL %1, request name %2.").arg(p1).arg(p2),
                    tr("Could not parse I/O module response. URL %1, request name %2.").arg(p1).arg(p2)
                );

                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsBase, device)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsRebootRestore, device)
                    << firmwareSupport;
                break;
            }
            case noMediaTrack:
            {
                errorMessageParts << tr("No supported media tracks at URL %1.").arg(p1)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsRebootRestore, device)
                    << firmwareSupport;
                break;
            }
            case notAuthorised:
            {
                errorMessageParts << tr("Not authorized. URL %1.").arg(p1);
                break;
            }
            case unsupportedProtocol:
            {
                errorMessageParts
                    << tr("Cannot open media URL %1. Unsupported media protocol %2.").arg(p1).arg(p2)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsRebootRestore, device)
                    << firmwareSupport;
                break;
            }
            case cannotConfigureMediaStream:
            {
                QnCameraDeviceStringSet detailsBase(
                    tr("If disabled, try to enable Recording and decrease FPS in Device Settings "
                        "(error \"%1\").").arg(p1),
                    tr("If disabled, try to enable Recording and decrease FPS in Camera Settings "
                        "(error \"%1\").").arg(p1),
                    tr("If disabled, try to enable Recording in I/O Module Settings "
                        "(error \"%1\").").arg(p1)
                );
                QnCameraDeviceStringSet detailsAdvanced(
                    tr("If it does not help, restore factory defaults on the device web-page."),
                    tr("If it does not help, restore factory defaults on the camera web-page."),
                    tr("If it does not help, restore factory defaults on the I/O module web-page.")
                );

                errorMessageParts
                    << tr("Failed to configure parameter %1.").arg(p1)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsBase, device)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsAdvanced, device)
                    << firmwareSupport;
                break;
            }
            case requestFailed:
            {
                QnCameraDeviceStringSet detailsBase(
                    tr("Device request \"%1\" failed with error \"%2\".").arg(p1).arg(p2),
                    tr("Camera request \"%1\" failed with error \"%2\".").arg(p1).arg(p2),
                    tr("I/O Module request \"%1\" failed with error \"%2\".").arg(p1).arg(p2)
                );
                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsBase, device)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsRebootRestore, device)
                    << firmwareSupport;
                break;
            }
            case notImplemented:
            {
                QnCameraDeviceStringSet details(
                    tr("Unknown device issue."),
                    tr("Unknown camera issue."),
                    tr("Unknown I/O module issue.")
                );

                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, details, device)
                    << tr("Please contact support.");
                break;
            }
            case ioError:
            {
                errorMessageParts
                    << tr("An input/output error has occurred. OS message: \"%1\".").arg(p1)
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, detailsPluggedReboot, device);
                break;
            }
            case serverTerminated:
            {
                errorMessageParts << tr("Server has been stopped.");
                break;
            }
            case cameraInvalidParams:
            {
                QnCameraDeviceStringSet details(
                    tr("Invalid data was received from the device %1.").arg(p1),
                    tr("Invalid data was received from the camera %1.").arg(p1),
                    tr("Invalid data was received from the I/O module %1.").arg(p1)
                );

                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, details, device);
                break;
            }
            case cameraOldFirmwareError:
            {
                errorMessageParts <<
                    tr("Please update firmware. "
                       "Minimal supported version is %1. Current version is %2").arg(p1).arg(p2);
                break;
            }
            case badMediaStream:
            {
                QnCameraDeviceStringSet details(
                    tr("Too many media errors. Please open device issues dialog for more details."),
                    tr("Too many media errors. Please open camera issues dialog for more details."),
                    tr("Too many media errors. Please open I/O module issues dialog for more details.")
                );

                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, details, device);
                break;
            }
            case noMediaStream:
            {
                errorMessageParts
                    << tr("Media stream is opened but no media data was received.");
                break;
            }
            case initializationInProgress:
            {
                QnCameraDeviceStringSet details(
                    tr("Device initialization process is in progress."),
                    tr("Camera initialization process is in progress."),
                    tr("I/O Module initialization process is in progress.")
                );

                errorMessageParts
                    << QnDeviceDependentStrings::getNameFromSet(resourcePool, details, device);
                break;
            }
            case pluginError:
            {
                errorMessageParts << tr("Plugin error. %1").arg(p1);
                break;
            }
            case pluginIsUnavailable:
            {
                errorMessageParts << tr("Plugin is unavailable");
                break;
            }
            case internalServerError:
            {
                errorMessageParts << tr("Internal server error. %1").arg(p1);
                break;
            }
            default:
            {
                QStringList nonEmptyParams = errorParams;
                nonEmptyParams.removeAll(QString());

                errorMessageParts << tr("Unknown error. Please contact support.");
                if (!nonEmptyParams.isEmpty())
                {
                    errorMessageParts << QString() << QString()
                        << tr("Parameters:")
                        << nonEmptyParams.join(", ");
                }
                break;
            }
        }

        return errorMessageParts.join('\n');
    }
};


namespace CameraDiagnostics {

namespace Step {

// TODO: #sivanov Use reflection.
QString toString(Value val)
{
    switch (val)
    {
        case none:
            return "none";
        case mediaServerAvailability:
            return "mediaServerAvailability";
        case cameraAvailability:
            return "cameraAvailability";
        case mediaStreamAvailability:
            return "mediaStreamAvailability";
        case mediaStreamIntegrity:
            return "mediaStreamIntegrity";
        default:
            return "unknown";
    }
}

Value fromString(const QString& str)
{
    if (str == "mediaServerAvailability")
        return mediaServerAvailability;
    else if (str == "cameraAvailability")
        return cameraAvailability;
    else if (str == "mediaStreamAvailability")
        return mediaStreamAvailability;
    else if (str == "mediaStreamIntegrity")
        return mediaStreamIntegrity;
    else
        return none;
}

} // namespace Step

namespace ErrorCode {

QString toString(
    Value val,
    QnResourcePool* resourcePool,
    const QnVirtualCameraResourcePtr &device,
    const ErrorParams& errorParams)
{
    return QnCameraDiagnosticsErrorCodeStrings::toString(val, resourcePool, device, errorParams);
}

QString toString(int val,
    QnResourcePool* resourcePool,
    const QnVirtualCameraResourcePtr &device,
    const ErrorParams& errorParams)
{
    return toString(static_cast<Value>(val), resourcePool, device, errorParams);
}

} // namespace ErrorCode


Result::Result():
    errorCode(ErrorCode::noError)
{
}

Result::Result(
    ErrorCode::Value _errorCode,
    const QString& param1,
    const QString& param2)
    :
    errorCode(_errorCode)
{
    errorParams.push_back(param1);
    errorParams.push_back(param2);
}

Result::operator bool() const
{
    return errorCode == ErrorCode::noError;
}

QString Result::toString(
    QnResourcePool* resourcePool) const
{
    return ErrorCode::toString(errorCode, resourcePool, {}, errorParams);
}

QString Result::toString(
    QnResourcePool* resourcePool,
    const QnVirtualCameraResourcePtr& device) const
{
    return ErrorCode::toString(errorCode, resourcePool, device, errorParams);
}

nx::vms::common::MediaStreamEvent Result::toMediaStreamEvent() const
{
    switch (errorCode)
    {
        case ErrorCode::tooManyOpenedConnections:
            return nx::vms::common::MediaStreamEvent::tooManyOpenedConnections;
        case ErrorCode::canNotDecryptMedia:
            return nx::vms::common::MediaStreamEvent::cannotDecryptMedia;
        default:
            return nx::vms::common::MediaStreamEvent::noEvent;
    }
}

QString Result::urlToStr(const nx::utils::Url& value) const
{
    return value.toString(QUrl::RemoveUserInfo);
}

} // namespace CameraDiagnostics
