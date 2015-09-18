/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#include "managers_types.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {


api::ResultCode fromDbResultCode( nx::db::DBResult dbResult )
{
    switch( dbResult )
    {
        case nx::db::DBResult::ok:
            return api::ResultCode::ok;

        case nx::db::DBResult::notFound:
            return api::ResultCode::notFound;

        case nx::db::DBResult::ioError:
            return api::ResultCode::dbError;
    }

    return api::ResultCode::dbError;
}


std::unique_ptr<stree::AbstractConstIterator> DataFilter::begin() const
{
    //TODO #ak
    return m_rc.begin();
}

bool DataFilter::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}

bool DataFilter::empty() const
{
    return m_fields.empty();
}

bool loadFromUrlQuery(const QUrlQuery& /*urlQuery*/, DataFilter* const /*dataFilter*/)
{
    //TODO #ak
    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DataFilter),
    (json),
    _Fields)

}   //cdb
}   //nx
