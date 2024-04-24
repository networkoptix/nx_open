#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <core/resource/resource.h>
#include <nx/vms/api/data/data_fwd.h>
#include <nx_ec/ec_api_common.h>

namespace ec2::detail {

struct ServerApiErrors
{
    Q_DECLARE_TR_FUNCTIONS(ServerApiErrors);
};

bool validateForbiddenSpaces(const QString& value);

bool validateNotEmptyWithoutSpaces(const QString& value);

bool validateUrlOrEmpty(const QString& value);

bool validateUrl(const QString& value);

Result invalidParameterError(const QString& paramName);

template<typename Key, typename Value>
Value* getMapValue(const std::map<Key, Value*>& map, const Key& key)
{
    auto it = map.find(key);
    if (it == map.end())
        return nullptr;
    return it->second;
}

template<typename Param, typename Existing>
bool validateName(const Param& param, const Existing* existing)
{
    if (existing == nullptr || existing->name != param.name)
        return validateNotEmptyWithoutSpaces(param.name);
    return true;
}

template<typename Param, typename MemberType, typename Functor>
bool validateField(
    const Param& param,
    const Param* existing,
    MemberType Param::* mptr,
    Functor checker)
{
    if (existing == nullptr || existing->*mptr != param.*mptr)
        return checker(param.*mptr);
    return true;
}

template<typename Param, typename MemberType>
bool validateFieldWithoutSpaces(
    const Param& param,
    const Param* existing,
    MemberType Param::* mptr)
{
    return validateField(param, existing, mptr, &validateForbiddenSpaces);
}

template<typename Param, typename Functor>
bool validateResourceName(const Param& data, const QnResourcePtr& existing, Functor validator)
{
    if (!existing || existing->getName() != data.name)
        return validator(data.name);
    return true;
}

template<typename Param>
bool validateResourceName(const Param& data, const QnResourcePtr& existing)
{
    return validateResourceName(data, existing, &validateNotEmptyWithoutSpaces);
}

template<typename Param>
bool validateResourceNameOrEmpty(const Param& data, const QnResourcePtr& existing)
{
    return validateResourceName(data, existing, &validateForbiddenSpaces);
}


template<typename Param, typename Functor>
bool validateResourceUrl(const Param& data, const QnResourcePtr& existing, Functor validator)
{
    if (!existing || existing->getUrl() != data.url)
        return validator(data.url);
    return true;
}

template<typename Param>
bool validateResourceUrl(const Param& data, const QnResourcePtr& existing)
{
    return validateResourceUrl(data, existing, &validateUrl);
}

template<typename Param>
bool validateResourceUrlOrEmpty(const Param& data, const QnResourcePtr& existing)
{
    return validateResourceUrl(data, existing, &validateUrlOrEmpty);
}

template<typename Param>
Result validateModifyResourceParam(const Param& /*data*/, const QnResourcePtr& /*existing*/)
{
    return {};
}

Result validateModifyResourceParam(const nx::vms::api::LayoutData& data, const QnResourcePtr& existing);
Result validateModifyResourceParam(const nx::vms::api::VideowallData& data, const QnResourcePtr& existing);
Result validateModifyResourceParam(const nx::vms::api::WebPageData& data, const QnResourcePtr& existing);
Result validateModifyResourceParam(const nx::vms::api::CameraData& data, const QnResourcePtr& existing);


} // namespace ec2::detail
