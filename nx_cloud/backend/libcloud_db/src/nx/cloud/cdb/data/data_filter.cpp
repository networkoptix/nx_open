#include "data_filter.h"

#include <nx/fusion/model_functions.h>

#include "../stree/stree_manager.h"

namespace nx {
namespace cdb {
namespace data {

bool DataFilter::getAsVariant(int resId, QVariant* const value) const
{
    const auto it = m_data.find(resId);
    if (it == m_data.end())
        return false;
    *value = it->second.front();
    return true;
}

bool DataFilter::empty() const
{
    return m_data.empty();
}

void DataFilter::addFilterValue(int resId, const QVariant& value)
{
    m_data[resId].push_back(value);
}

bool DataFilter::resourceValueMatches(int resId, const QVariant& value) const
{
    const auto it = m_data.find(resId);
    if (it == m_data.end())
        return false;

    return std::find(it->second.begin(), it->second.end(), value) != it->second.end();
}

bool DataFilter::matches(const nx::utils::stree::AbstractResourceReader& record) const
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it)
    {
        const auto actual = record.get(it->first);
        if (!actual)
            return false;
        if (std::find(it->second.begin(), it->second.end(), *actual) == it->second.end())
            return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

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
        dataFilter->addFilterValue(resDescription.id, std::move(val));
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
        dataFilter->addFilterValue(resDescription.id, std::move(val));
    }

    return true;
}

} // namespace data
} // namespace cdb
} // namespace nx
