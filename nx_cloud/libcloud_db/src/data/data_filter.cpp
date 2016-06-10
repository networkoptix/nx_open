/**********************************************************
* Oct 12, 2015
* a.kolesnikov
***********************************************************/

#include "data_filter.h"

#include <nx/fusion/model_functions.h>

#include "stree/stree_manager.h"


namespace nx {
namespace cdb {
namespace data {

std::unique_ptr<stree::AbstractConstIterator> DataFilter::begin() const
{
    return m_rc.begin();
}

bool DataFilter::getAsVariant(int resID, QVariant* const value) const
{
    return m_rc.getAsVariant(resID, value);
}

bool DataFilter::empty() const
{
    return m_rc.empty();
}

stree::ResourceContainer& DataFilter::resources()
{
    return m_rc;
}

const stree::ResourceContainer& DataFilter::resources() const
{
    return m_rc;
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter)
{
    const auto queryItems = urlQuery.queryItems();
    for (const auto& item: queryItems)
    {
        const auto resDescription = 
            StreeManager::instance()->resourceNameSet().findResourceByName(item.first);
        if (resDescription.id == stree::INVALID_RES_ID)
            continue;
        QVariant val(item.second);
        val.convert(resDescription.type);
        dataFilter->m_rc.put(resDescription.id, std::move(val));
    }

    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DataFilter),
    (json),
    _Fields)

}   //data
}   //cdb
}   //nx
