/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_MANAGERS_TYPES_H
#define CLOUD_DB_MANAGERS_TYPES_H

#include <map>
#include <string>

#include <QtCore/QVariant>  //TODO #ak maybe boost::variant?

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/db/types.h>


namespace nx {
namespace cdb {

enum class ResultCode
{
    ok,
    notAuthorized,
    notFound,
    alreadyExists,
    dbError,
    notImplemented
};

ResultCode fromDbResultCode( nx::db::DBResult );

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
:
    public stree::AbstractIteratableContainer,
    public stree::AbstractResourceReader
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

#endif  //CLOUD_DB_MANAGERS_TYPES_H
