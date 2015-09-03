/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_MANAGERS_TYPES_H
#define CLOUD_DB_MANAGERS_TYPES_H

#include <map>
#include <string>

#include <QtCore/QVariant>  //TODO #ak maybe boost::variant?
#include <QtCore/QUrlQuery>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/db/types.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/result_code.h>


namespace nx {
namespace cdb {

api::ResultCode fromDbResultCode( nx::db::DBResult );

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
    //!Implementation of \a stree::AbstractIteratableContainer::begin
    virtual std::unique_ptr<stree::AbstractConstIterator> begin() const override;
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

    //!Empty filter means data should not be filtered
    bool empty() const;

    //TODO #ak do something with this class
    int dummy;

private:
    stree::ResourceContainer m_rc;
    //!map<field name, field value>
    std::map<std::string, QVariant> m_fields;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter);

#define DataFilter_Fields (dummy)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (DataFilter),
    (json))

}   //cdb
}   //nx

#endif  //CLOUD_DB_MANAGERS_TYPES_H
