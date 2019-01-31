#pragma once

#include <functional>
#include <vector>

#include <QtCore/QString>

#include <core/resource_access/user_access_data.h>
#include <core/resource_access/providers/resource_access_provider.h>

namespace ec2 {
namespace access_helpers {

enum class Mode
{
    read,
    write
};

using KeyValueFilterType = std::pair<const QString&, QString*>;
using FilterFunctorType = std::function<bool(Mode mode, const Qn::UserAccessData&, KeyValueFilterType*)>;
using FilterFunctorListType = std::vector<FilterFunctorType>;

namespace detail {
std::vector<QString> getRestrictedKeysByMode(Mode mode);
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, KeyValueFilterType* keyValue);
bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key, QString* value);
bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key);

void applyValueFilters(
    Mode mode,
    const Qn::UserAccessData& accessData,
    KeyValueFilterType* keyValue,
    const FilterFunctorListType& filterList,
    bool* allowed = nullptr);

template<typename ResourceType>
QnSharedResourcePointerList<ResourceType> getAccessibleResources(
    const QnUserResourcePtr& subject,
    QnSharedResourcePointerList<ResourceType> resources,
    QnResourceAccessProvider* accessProvider)
{
    const auto itEnd = std::remove_if(resources.begin(), resources.end(),
        [accessProvider, subject](const QnSharedResourcePointer<ResourceType>& other)
        {
            return !accessProvider->hasAccess(subject, other);
        });
    resources.erase(itEnd, resources.end());
    return resources;
}

} // namespace access_helpers
} // namespace ec2
