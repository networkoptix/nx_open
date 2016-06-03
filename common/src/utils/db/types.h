/**********************************************************
* aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_DB_TYPES_H
#define NX_CLOUD_DB_DB_TYPES_H

#include <chrono>

#include <QtCore/QString>


namespace nx {
namespace db {

enum class DBResult
{
    ok,
    statementError,
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
    /** connection is closed if not used for this interval */
    std::chrono::seconds inactivityTimeout;

    ConnectionOptions()
    :
        port(0),
        maxConnectionCount(1),
        inactivityTimeout(std::chrono::minutes(10))
    {
    }
};

}   //db
}   //nx

#endif  //NX_CLOUD_DB_DB_TYPES_H
