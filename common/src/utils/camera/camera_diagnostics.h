/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_DIAGNOSTICS_H
#define CAMERA_DIAGNOSTICS_H

#include <QList>
#include <QString>


//!Holds types related to performing camera availability diagnostics
namespace CameraDiagnostics
{
    namespace Step
    {
        // TODO: #Elric #enum
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

        QString toString( Value val );
        Value fromString( const QString& str );
    }

    //!Contains error codes
    namespace ErrorCode
    {
        // TODO: #Elric #enum
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
            cameraInitializationInProgress,
            unknown
        };

        //!Returns textual description of error with  parameters
        QString toString( Value val, const QList<QString>& errorParams = QList<QString>() );
        QString toString( int val, const QList<QString>& errorParams = QList<QString>() );
    }

    /*!
        It is strongly recommended to use inherited classes.
    */
    class Result
    {
    public:
        typedef void (Result::*safe_bool_type)() const;

        ErrorCode::Value errorCode;
        QList<QString> errorParams;

        Result();
        explicit Result( ErrorCode::Value _errorCode, const QString& param1 = QString(), const QString& param2 = QString() );

        QString toString() const;
        operator safe_bool_type() const;

    private:
        void safe_bool_type_retval() const {}
    };

    class NoErrorResult : public Result
    {
    public:
        NoErrorResult() : Result( ErrorCode::noError ) {}
    };

    class MediaServerUnavailableResult : public Result
    {
    public:
        MediaServerUnavailableResult( const QString& mediaServerIP ) : Result( ErrorCode::mediaServerUnavailable, mediaServerIP ) {}
    };

    class MediaServerBadResponseResult : public Result
    {
    public:
        MediaServerBadResponseResult( const QString& mediaServerIP, const QString& serverResponse ) : Result( ErrorCode::mediaServerBadResponse, mediaServerIP, serverResponse ) {}
    };

    class CannotEstablishConnectionResult : public Result
    {
    public:
        CannotEstablishConnectionResult( const int port ) : Result( ErrorCode::cannotEstablishConnection, QString::number(port) ) {}
    };

    class CannotOpenCameraMediaPortResult : public Result
    {
    public:
        CannotOpenCameraMediaPortResult( const QString& mediaURL, const int mediaPort ) : Result( ErrorCode::cannotOpenCameraMediaPort, mediaURL, QString::number(mediaPort) ) {}
    };

    class ConnectionClosedUnexpectedlyResult : public Result
    {
    public:
        ConnectionClosedUnexpectedlyResult( const QString& mediaURL, const int mediaPort )
            : Result( ErrorCode::connectionClosedUnexpectedly, mediaURL, QString::number(mediaPort) ) {}
    };

    class BadMediaStreamResult : public Result
    {
    public:
        BadMediaStreamResult()
            : Result( ErrorCode::badMediaStream) {}
    };

    class NoMediaStreamResult : public Result
    {
    public:
        NoMediaStreamResult()
            : Result( ErrorCode::noMediaStream) {}
    };

    class CameraResponseParseErrorResult : public Result
    {
    public:
        CameraResponseParseErrorResult( const QString& requestedURL, const QString requestName ) : Result( ErrorCode::responseParseError, requestedURL, requestName ) {}
    };

    class NoMediaTrackResult : public Result
    {
    public:
        NoMediaTrackResult( const QString& requestedURL ) : Result( ErrorCode::noMediaTrack, requestedURL ) {}
    };

    class NotAuthorisedResult : public Result
    {
    public:
        NotAuthorisedResult( const QString& requestedURL ) : Result( ErrorCode::notAuthorised, requestedURL ) {}
    };

    class UnsupportedProtocolResult : public Result
    {
    public:
        UnsupportedProtocolResult( const QString& mediaURL, const QString& protocolName ) : Result( ErrorCode::unsupportedProtocol, mediaURL, protocolName ) {}
    };

    class CannotConfigureMediaStreamResult : public Result
    {
    public:
        CannotConfigureMediaStreamResult( const QString& failedParamName ) : Result( ErrorCode::cannotConfigureMediaStream, failedParamName ) {}
    };

    class RequestFailedResult : public Result
    {
    public:
        RequestFailedResult( const QString& requestName, const QString& serverResponse ) : Result( ErrorCode::requestFailed, requestName, serverResponse ) {}
    };

    class CameraInvalidParams : public Result
    {
    public:
        CameraInvalidParams( const QString& errorString) : Result( ErrorCode::cameraInvalidParams, errorString) {}
    };

    class NotImplementedResult : public Result
    {
    public:
        NotImplementedResult() : Result( ErrorCode::notImplemented ) {}
    };

    class IOErrorResult : public Result
    {
    public:
        IOErrorResult( const QString& osErrorMessage ) : Result( ErrorCode::ioError, osErrorMessage ) {}
    };

    class UnknownErrorResult : public Result
    {
    public:
        UnknownErrorResult() : Result( ErrorCode::unknown ) {}
    };

    class InitializationInProgress : public Result
    {
    public:
        InitializationInProgress() : Result( ErrorCode::cameraInitializationInProgress ) {}
    };

    class ServerTerminatedResult : public Result
    {
    public:
        ServerTerminatedResult() : Result( ErrorCode::serverTerminated ) {}
    };
}

#endif  //CAMERA_DIAGNOSTICS_H
