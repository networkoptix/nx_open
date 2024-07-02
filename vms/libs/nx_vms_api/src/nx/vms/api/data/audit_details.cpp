// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_details.h"

#include <nx/fusion/model_functions.h>

#include "software_version_serialization.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SessionDetails, (ubjson)(json), SessionDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PlaybackDetails, (ubjson)(json), PlaybackDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceDetails, (ubjson)(json), DeviceDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResourceDetails, (ubjson)(json), ResourceDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DescriptionDetails, (ubjson)(json), DescriptionDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MitmDetails, (ubjson)(json), MitmDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateDetails, (ubjson)(json), UpdateDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ProxySessionDetails, (ubjson)(json), ProxySessionDetails_Fields)

void serialize_field(const AllAuditDetails::type& data, QVariant* target)
{
    QByteArray tmp;
    QnJsonContext jsonContext;
    jsonContext.setChronoSerializedAsDouble(true);
    jsonContext.setSerializeMapToObject(true);
    QJson::serialize(&jsonContext, data, &tmp);
    serialize_field(tmp, target);
}

void deserialize_field(const QVariant& value, AllAuditDetails::type* target)
{
    QByteArray tmp;
    QnJsonContext jsonContext;
    jsonContext.setStrictMode(true);
    deserialize_field(value, &tmp);
    NX_ASSERT(QJson::deserialize(&jsonContext, tmp, target));
}

void serialize(const AllAuditDetails::type& data, QnUbjsonWriter<QByteArray>* stream)
{
    QnUbjson::serialize((int)data.index(), stream);
    std::visit(
        [&](auto& details)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(details)>, std::nullptr_t>)
                return;
            else
                QnUbjson::serialize(details, stream);
        },
        data);
}

template<typename Func, typename Variant, std::size_t... I>
bool deserializeAlternative(std::index_sequence<I...> idx, Func&& f, Variant* variant)
{
    *variant = std::variant_alternative_t<idx.size(), Variant>();
    return f(std::get_if<idx.size()>(variant));
}

template<typename Func, typename Variant, std::size_t... I>
auto switchByAlternativeIndex(int alternative, std::index_sequence<I...>, Func&& f, Variant* variant)
{
    bool ret = false;
    const auto success =
        (((alternative == I) && ((ret = deserializeAlternative(std::make_index_sequence<I>(),
            std::forward<Func>(f), variant)), true))
        || ...);
    return success && ret;
}

bool deserialize(QnUbjsonReader<QByteArray>* stream, AllAuditDetails::type* target)
{
    int index;
    if (!QnUbjson::deserialize(stream, &index))
        return false;

    return switchByAlternativeIndex(index,
        std::make_index_sequence<std::variant_size_v<AllAuditDetails::type>>(),
        [&](auto details)
        {
            if constexpr (std::is_same_v<decltype(details),std::nullptr_t*>)
            {
                return true;
            }
            else
            {
                if (!QnUbjson::deserialize(stream, details))
                    return false;
                return true;
            }
        },
        target);
}

} // namespace nx::vms::api
