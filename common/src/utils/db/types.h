/**********************************************************
* aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_DB_TYPES_H
#define NX_CLOUD_DB_DB_TYPES_H

#include <QtCore/QString>


namespace nx {
namespace db {


enum class DBResult
{
    ok,
    ioError,
    notFound
};


class ConnectionOptions
{
public:
    QString driverName;
    QString hostName;
    int port;
    QString dbName;
    QString userName;
    QString password;
    QString connectOptions;
    size_t maxConnectionCount;

    ConnectionOptions()
    :
        driverName( lit("QMYSQL") ),
        port( 0 ),
        maxConnectionCount( 1 )
    {
    }
};


}   //db
}   //nx

#endif  //NX_CLOUD_DB_DB_TYPES_H
