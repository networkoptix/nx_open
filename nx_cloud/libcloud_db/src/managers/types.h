/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_types_h
#define cloud_db_types_h

#include <map>
#include <string>

#include <QtCore/QVariant>  //TODO #ak maybe boost::variant?


namespace nx {
namespace cdb {

enum class ResultCode
{
    ok,
    notAuthorized,
    notFound,
    alreadyExists,
    dbError
};

enum class EntityType
{
    none,
    account,
    system,
    subscription,
    product
    //...
};

enum class DataActionType
{
    fetch,
    insert,
    update,
    _delete
};


class DataFilter
{
public:
    //!Empty filter means data should not be filtered
    bool empty() const;

private:
    //!map<field name, field value>
    std::map<std::string, QVariant> m_fields;
};

}   //cdb
}   //nx

#endif
