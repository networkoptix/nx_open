#include "access_helpers.h"

#include <api/global_settings.h>
#include <core/resource/param.h>

namespace ec2 {
namespace access_helpers {

namespace detail {

std::vector<QString> getRestrictedKeysByMode(Mode mode)
{
    using namespace nx::settings_names;

    switch (mode)
    {
        case Mode::read:
            return {
                kNamePassword,
                ldapAdminPassword,
                kNameCloudAuthKey,
                Qn::CAMERA_CREDENTIALS_PARAM_NAME,
                Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME
            };
        case Mode::write:
            return {
                kNameCloudSystemId,
                kNameCloudAuthKey,
                kCloudHostName,
                kNameLocalSystemId
            };
    }

    return std::vector<QString>();
}

} // namespace detail

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, KeyValueFilterType* keyValue)
{
    auto in_ = [](const std::vector<QString>& vec, const QString& value)
    {
        return std::any_of(vec.cbegin(), vec.cend(), [&value](const QString& s) { return s == value; });
    };

    if (in_(detail::getRestrictedKeysByMode(mode), keyValue->first) && accessData != Qn::kSystemAccess)
        return false;

    return true;
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key)
{
    QString dummy = lit("dummy");

    KeyValueFilterType kv(key, &dummy);
    return kvSystemOnlyFilter(mode, accessData, &kv);
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key, QString* value)
{
    KeyValueFilterType kv(key, value);
    return kvSystemOnlyFilter(mode, accessData, &kv);
}

void applyValueFilters(
    Mode mode,
    const Qn::UserAccessData& accessData,
    KeyValueFilterType* keyValue,
    const FilterFunctorListType& filterList,
    bool* allowed)
{
    if (allowed)
        *allowed = true;

    for (auto filter : filterList)
    {
        bool isAllowed = filter(mode, accessData, keyValue);

        if (allowed && !isAllowed)
            *allowed = false;
    }
}

} // namespace access_helpers
} // namespace ec2
