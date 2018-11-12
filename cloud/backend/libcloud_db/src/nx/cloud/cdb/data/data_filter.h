#pragma once

#include <map>
#include <vector>

#include <QtCore/QUrlQuery>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/stree/resourcecontainer.h>

namespace nx {
namespace cdb {
namespace data {

class DataFilter:
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resId, QVariant* const value) const override;

    /**
     * Empty filter means data should not be filtered.
     */
    bool empty() const;
    /**
     * Adds another allowed value for resource resId.
     */
    void addFilterValue(int resId, const QVariant& value);
    /**
     * @return true if specified value of resource resId has been found.
     */
    bool resourceValueMatches(int resId, const QVariant& value) const;
    bool matches(const nx::utils::stree::AbstractResourceReader& record) const;

private:
    std::map<int, std::vector<QVariant>> m_data;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, DataFilter* const dataFilter);
bool deserialize(QnJsonContext*, const QJsonValue&, DataFilter*);

} // namespace data
} // namespace cdb
} // namespace nx
