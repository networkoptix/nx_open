/**********************************************************
* Oct 12, 2015
* a.kolesnikov
***********************************************************/

#include "data_filter.h"

#include <nx/fusion/model_functions.h>

#include "../stree/stree_manager.h"


namespace nx {
namespace cdb {
namespace data {

std::unique_ptr<nx::utils::stree::AbstractConstIterator> DataFilter::begin() const
{
    return m_rc.begin();
}

bool DataFilter::getAsVariant(int resID, QVariant* const value) const
{
    return m_rc.getAsVariant(resID, value);
}

void DataFilter::put(int resID, const QVariant& value)
{
    m_rc.put(resID, value);
}

bool DataFilter::empty() const
{
    return m_rc.empty();
}

nx::utils::stree::ResourceContainer& DataFilter::resources()
{
    return m_rc;
}

const nx::utils::stree::ResourceContainer& DataFilter::resources() const
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
        if (resDescription.id == nx::utils::stree::INVALID_RES_ID)
            continue;
        QVariant val(item.second);
        val.convert(resDescription.type);
        dataFilter->put(resDescription.id, std::move(val));
    }

    return true;
}

bool deserialize(QnJsonContext*, const QJsonValue& value, DataFilter* dataFilter)
{
    if (value.type() != QJsonValue::Object)
        return false;

    const QJsonObject jsonMap = value.toObject();
    for (auto it = jsonMap.begin(); it != jsonMap.end(); ++it)
    {
        const auto resDescription =
            StreeManager::instance()->resourceNameSet().findResourceByName(it.key());
        if (resDescription.id == nx::utils::stree::INVALID_RES_ID)
            continue;
        QVariant val(it.value());
        val.convert(resDescription.type);
        dataFilter->put(resDescription.id, std::move(val));
    }

    return true;
}

}   //data
}   //cdb
}   //nx
