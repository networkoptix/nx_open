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
        enum Value
        {
            none = 0,
            //!checking media server availability
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

    namespace ErrorCode
    {
        enum Value
        {
            noError = 0,
            //!params: port
            cannotEstablishConnection,
            //!params: mediaPort
            cannotOpenCameraMediaPort,
            //!params: mediaPort
            connectionClosedUnexpectedly,
            //!no params
            responseParseError,
            //!no params
            noMediaTrack,
            //!no params
            notAuthorised,
            //!params: protocolName
            unsupportedProtocol,
            //!params: failed param
            cannotConfigureMediaStream,
            //!params: request name, server response message
            requestFailed,
            notImplemented,
            //!params: OS error message
            ioError,
            serverTerminated,
            unknown
        };

        //!Returns textual description of error with  parameters
        QString toString( Value val, const QList<QString>& errorParams = QList<QString>() );
        QString toString( int val, const QList<QString>& errorParams = QList<QString>() );
    }

    class Result
    {
    public:
        ErrorCode::Value errorCode;
        QList<QString> errorParams;

        Result();
        Result( ErrorCode::Value _errorCode, const QString& param1 = QString(), const QString& param2 = QString() );
        QString toString() const;
        operator bool() const;
    };

    class NoErrorResult : public Result
    {
    public:
        NoErrorResult() : Result( ErrorCode::noError ) {}
    };

    class CannotEstablishConnectionResult : public Result
    {
    public:
        CannotEstablishConnectionResult( const int port ) : Result( ErrorCode::cannotEstablishConnection, QString::number(port) ) {}
    };

    class CannotOpenCameraMediaPortResult : public Result
    {
    public:
        CannotOpenCameraMediaPortResult( const int mediaPort ) : Result( ErrorCode::cannotOpenCameraMediaPort, QString::number(mediaPort) ) {}
    };

    class ConnectionClosedUnexpectedlyResult : public Result
    {
    public:
        ConnectionClosedUnexpectedlyResult( const int mediaPort ) : Result( ErrorCode::connectionClosedUnexpectedly, QString::number(mediaPort) ) {}
    };

    class CameraResponseParseErrorResult : public Result
    {
    public:
        CameraResponseParseErrorResult() : Result( ErrorCode::responseParseError ) {}
    };

    class NoMediaTrackResult : public Result
    {
    public:
        NoMediaTrackResult() : Result( ErrorCode::noMediaTrack ) {}
    };

    class NotAuthorisedResult : public Result
    {
    public:
        NotAuthorisedResult() : Result( ErrorCode::notAuthorised ) {}
    };

    class UnsupportedProtocolResult : public Result
    {
    public:
        UnsupportedProtocolResult( const QString& protocolName ) : Result( ErrorCode::unsupportedProtocol, protocolName ) {}
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

    class ServerTerminatedResult : public Result
    {
    public:
        ServerTerminatedResult() : Result( ErrorCode::serverTerminated ) {}
    };
}

#endif  //CAMERA_DIAGNOSTICS_H
