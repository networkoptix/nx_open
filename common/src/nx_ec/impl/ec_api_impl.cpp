/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec_api_impl.h"

#include <QtCore/QMetaType>


namespace ec2
{
    QString toString( ErrorCode errorCode )
    {
        switch( errorCode )
        {
            case ErrorCode::ok:
                return lit( "ok" );
            case ErrorCode::failure:
                return lit( "failure" );
            case ErrorCode::ioError:
                return lit( "IO error" );
            case ErrorCode::serverError:
                return lit( "server error" );
            case ErrorCode::unsupported:
                return lit( "unsupported" );
            case ErrorCode::unauthorized:
                return lit( "unauthorized" );
            case ErrorCode::badResponse:
                return lit( "badResponse" );
            case ErrorCode::dbError:
                return lit( "dbError" );
            case ErrorCode::containsBecauseTimestamp:
                return lit ("containsBecauseTimestamp");
            case ErrorCode::containsBecauseSequence:
                return lit ("containsBecauseSequence");
            case ErrorCode::notImplemented:
                return lit( "notImplemented" );
            default:
                return lit( "unknown error" );
        }
    }
}
