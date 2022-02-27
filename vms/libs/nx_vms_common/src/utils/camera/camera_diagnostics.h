// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QList>
#include <QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>
#include <nx/vms/common/globals/media_stream_event.h>

//!Holds types related to performing camera availability diagnostics
namespace CameraDiagnostics {

namespace Step {

enum Value
{
    none = 0,
    //!checking server availability
    mediaServerAvailability,
    //!checking that camera responses on base API requests
    cameraAvailability,
    //!checking that camera provides media stream
    mediaStreamAvailability,
    //!checking media stream provided by camera for errors
    mediaStreamIntegrity,
    end
};

NX_VMS_COMMON_API QString toString(Value val);
NX_VMS_COMMON_API Value fromString(const QString& str);

} // namespace Step

//!Contains error codes
namespace ErrorCode {

enum Value
{
    noError = 0,
    //!params: server ip
    mediaServerUnavailable,
    //!params: server ip
    mediaServerBadResponse,
    //!params: port
    cannotEstablishConnection,
    //!params: mediaURL, mediaPort
    cannotOpenCameraMediaPort,
    //!params: mediaPort
    connectionClosedUnexpectedly,
    //!no params
    responseParseError,
    //!no params
    noMediaTrack,
    //!no params
    notAuthorised,
    //!params: mediaURL, protocolName
    unsupportedProtocol,
    //!params: failed param
    cannotConfigureMediaStream,
    //!params: request name, server response message
    requestFailed,
    notImplemented,
    //!params: OS error message
    ioError,
    serverTerminated,
    cameraInvalidParams,
    badMediaStream,
    noMediaStream,
    initializationInProgress,
    pluginError,
    liveVideoIsNotSupportedError,
    tooManyOpenedConnections,
    cameraOldFirmwareError,
    pluginIsUnavailable,
    internalServerError,
    canNotDecryptMedia,
    unknown
};

//!Returns textual description of error with  parameters

typedef QList<QString> ErrorParams;

NX_VMS_COMMON_API QString toString(Value val,
    QnResourcePool* resourcePool,
    const QnVirtualCameraResourcePtr &device,
    const ErrorParams& errorParams = ErrorParams());

NX_VMS_COMMON_API QString toString(int val,
    QnResourcePool* resourcePool,
    const QnVirtualCameraResourcePtr &device,
    const ErrorParams& errorParams = ErrorParams());

} // namespace ErrorCode

/*!
    It is strongly recommended to use inherited classes.
*/
class NX_VMS_COMMON_API Result
{
public:
    ErrorCode::Value errorCode;
    QList<QString> errorParams;

    Result();
    explicit Result(
        ErrorCode::Value _errorCode,
        const QString& param1 = QString(),
        const QString& param2 = QString());

    explicit operator bool() const;
    QString toString(QnResourcePool* resourcePool) const;
    QString toString(QnResourcePool* resourcePool, const QnVirtualCameraResourcePtr& device) const;
    nx::vms::common::MediaStreamEvent toMediaStreamEvent() const;
protected:
    QString urlToStr(const nx::utils::Url& value) const;
};

class NoErrorResult: public Result
{
public:
    NoErrorResult():
        Result(ErrorCode::noError)
    {
    }
};

class MediaServerUnavailableResult: public Result
{
public:
    MediaServerUnavailableResult(
        const QString& mediaServerIP)
        :
        Result(ErrorCode::mediaServerUnavailable, mediaServerIP)
    {
    }
};

class MediaServerBadResponseResult: public Result
{
public:
    MediaServerBadResponseResult(
        const QString& mediaServerIP,
        const QString& serverResponse)
        :
        Result(ErrorCode::mediaServerBadResponse, mediaServerIP, serverResponse)
    {
    }
};

class CannotEstablishConnectionResult: public Result
{
public:
    CannotEstablishConnectionResult(
        const int port)
        :
        Result(ErrorCode::cannotEstablishConnection, QString::number(port))
    {
    }
};

class CannotOpenCameraMediaPortResult: public Result
{
public:
    CannotOpenCameraMediaPortResult(
        const nx::utils::Url& mediaURL,
        const int mediaPort)
        :
        Result(ErrorCode::cannotOpenCameraMediaPort, urlToStr(mediaURL), QString::number(mediaPort))
    {
    }
};

class ConnectionClosedUnexpectedlyResult: public Result
{
public:
    ConnectionClosedUnexpectedlyResult(
        const QString& host,
        const int mediaPort)
        :
        Result(ErrorCode::connectionClosedUnexpectedly, host, QString::number(mediaPort))
    {
    }
};

class BadMediaStreamResult: public Result
{
public:
    BadMediaStreamResult()
        : Result(ErrorCode::badMediaStream)
    {
    }
};

class NoMediaStreamResult: public Result
{
public:
    NoMediaStreamResult()
        : Result(ErrorCode::noMediaStream)
    {
    }
};

class LiveVideoIsNotSupportedResult: public Result
{
public:
    LiveVideoIsNotSupportedResult():
        Result(ErrorCode::liveVideoIsNotSupportedError)
    {
    }
};

class CameraResponseParseErrorResult: public Result
{
public:
    CameraResponseParseErrorResult(
        const nx::utils::Url& requestedURL,
        const QString &requestName)
        :
        Result(ErrorCode::responseParseError, urlToStr(requestedURL), requestName)
    {
    }
};

class NoMediaTrackResult: public Result
{
public:
    NoMediaTrackResult(
        const nx::utils::Url& requestedURL)
        :
        Result(ErrorCode::noMediaTrack, urlToStr(requestedURL))
    {
    }
};

class NotAuthorisedResult: public Result
{
public:
    NotAuthorisedResult(
        const nx::utils::Url& requestedURL)
        :
        Result(ErrorCode::notAuthorised, urlToStr(requestedURL))
    {
    }
};

class UnsupportedProtocolResult: public Result
{
public:
    UnsupportedProtocolResult(
        const nx::utils::Url& mediaUrl,
        const QString& protocolName)
        :
        Result(ErrorCode::unsupportedProtocol, urlToStr(mediaUrl), protocolName)
    {
    }
};

class CannotConfigureMediaStreamResult: public Result
{
public:
    CannotConfigureMediaStreamResult(
        const QString& failedParamName)
        :
        Result(ErrorCode::cannotConfigureMediaStream, failedParamName)
    {
    }
};

class RequestFailedResult: public Result
{
public:
    RequestFailedResult(
        const QString& requestName,
        const QString& serverResponse)
        :
        Result(ErrorCode::requestFailed, requestName, serverResponse)
    {
    }
};

class CameraInvalidParams: public Result
{
public:
    CameraInvalidParams(
        const QString& errorString)
        :
        Result(ErrorCode::cameraInvalidParams, errorString)
    {
    }
};

class CameraOldFirmware: public Result
{
public:
    CameraOldFirmware(const QString& minimalVersion, const QString& currentVersion):
        Result(ErrorCode::cameraOldFirmwareError,minimalVersion,currentVersion)
    {
    }
};

class NotImplementedResult: public Result
{
public:
    NotImplementedResult():
        Result(ErrorCode::notImplemented)
    {
    }
};

class IOErrorResult: public Result
{
public:
    IOErrorResult(
        const QString& osErrorMessage)
        :
        Result(ErrorCode::ioError, osErrorMessage)
    {
    }
};

class UnknownErrorResult: public Result
{
public:
    UnknownErrorResult():
        Result(ErrorCode::unknown)
    {
    }
};

class InitializationInProgress: public Result
{
public:
    InitializationInProgress():
        Result(ErrorCode::initializationInProgress)
    {
    }
};

class ServerTerminatedResult: public Result
{
public:
    ServerTerminatedResult():
        Result(ErrorCode::serverTerminated)
    {
    }
};

class PluginErrorResult: public Result
{
public:
    PluginErrorResult(const QString& errorMessage): Result(ErrorCode::pluginError, errorMessage) {}
};

class TooManyOpenedConnectionsResult: public Result
{
public:
    TooManyOpenedConnectionsResult()
        :
        Result(ErrorCode::tooManyOpenedConnections)
    {
    }
};

class PluginIsUnavailableResult: public Result
{
public:
    PluginIsUnavailableResult(): Result(ErrorCode::pluginIsUnavailable) {}
};

class InternalServerErrorResult: public Result
{
public:
    InternalServerErrorResult(const QString& errorMessage):
        Result(ErrorCode::internalServerError, errorMessage)
    {
    };
};

} // namespace CameraDiagnostics
