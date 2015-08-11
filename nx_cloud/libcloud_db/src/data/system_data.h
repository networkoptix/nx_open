/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_SYSTEM_DATA_H
#define CLOUD_DB_SYSTEM_DATA_H

#include <string>

#include <utils/common/uuid.h>


namespace nx {
namespace cdb {
namespace data {


class SubscriptionData
{
public:
    QnUuid productID;
    QnUuid systemID;
};

class SystemData
{
public:
    QnUuid id;
    std::string name;
    bool cloudConnectionSubscriptionStatus;
    //TODO
};


}   //data
}   //cdb
}   //nx

#endif //CLOUD_DB_SYSTEM_DATA_H
