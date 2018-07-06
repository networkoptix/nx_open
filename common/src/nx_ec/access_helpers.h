#pragma once

#include <functional>
#include <vector>

#include <QtCore/QString>

#include <core/resource_access/user_access_data.h>

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

} // namespace access_helpers
} // namespace ec2
