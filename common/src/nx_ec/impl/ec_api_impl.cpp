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
                return QString::fromLatin1( "ok" );
            case ErrorCode::failure:
                return QString::fromLatin1( "failure" );
            case ErrorCode::unsupported:
                return QString::fromLatin1( "unsupported" );
            case ErrorCode::ioError:
                return QString::fromLatin1( "IO error" );
            case ErrorCode::serverError:
                return QString::fromLatin1( "server error" );
            case ErrorCode::unauthorized:
                return QString::fromLatin1( "unauthorized" );
            case ErrorCode::badResponse:
                return QString::fromLatin1( "badResponse" );
            default:
                return QString::fromLatin1( "unknown error" );
        }
    }
}
