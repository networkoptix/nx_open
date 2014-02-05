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
            default:
                return QString::fromLatin1( "unknown error" );
        }
    }
}
