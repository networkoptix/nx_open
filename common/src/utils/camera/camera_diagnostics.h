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
    namespace DiagnosticsStep
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
            //!params: cameraIP
            cannotEstablishConnection,
            //!params: cameraIP, mediaPort
            cannotOpenCameraMediaPort,
            connectionClosedUnexpectedly,
            responseParseError,
            noMediaTrack,
            notAuthorised,
            unsupportedProtocol,
            cannotConfigureMediaStream
        };

        //!Returns textual description of error with  parameters
        QString toString( Value val, const QList<QString>& errorParams = QList<QString>() );
        QString toString( int val, const QList<QString>& errorParams = QList<QString>() );
    }
}

#endif  //CAMERA_DIAGNOSTICS_H
