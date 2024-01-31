// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WebPageModelV1, (json), WebPageModelV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WebPageModelV3, (json), WebPageModelV3_Fields)

namespace {

const QString kProxyDomainAllowListPropertyName = "proxyDomainAllowList";
const QString kCertificateCheckPropertyName = "certificateCheck";

template <typename Model>
std::vector<Model> fromData(std::vector<WebPageData> pages)
{
    std::vector<Model> result{};
    result.reserve(pages.size());
    for (auto&& p: pages)
    {
        Model m;
        m.id = p.id;
        m.name = std::move(p.name);
        m.url = std::move(p.url);

        result.emplace_back(std::move(m));
    }

    return result;
}

template <typename Model>
WebPageModelV1::DbUpdateTypes toDbTypes(Model m)
{
    WebPageData data;
    data.id = std::move(m.id);
    data.name = std::move(m.name);
    data.url = std::move(m.url);

    if constexpr (std::is_same_v<Model, WebPageModelV3>)
    {
        // As of this writing `webpage_resource.cpp` expects a QString that is convertible to uint.
        m.parameters[kCertificateCheckPropertyName] =
            toString(static_cast<uint>(m.certificateCheck));

        QJson::serialize(m.proxyDomainAllowList, &m.parameters[kProxyDomainAllowListPropertyName]);
    }

    return {
        std::move(data),
        static_cast<ResourceWithParameters&>(m).asList(data.id),
    };
}

template <typename Model>
std::vector<Model> fromDbTypes(typename Model::DbListTypes all)
{
    // Ignore Clang-Tidy warnings about `'all' is used after being moved`.
    // To move an element from `std::tuple`, a specific overload`T&& std::get(tuple&&)`
    // must be selected.
    // `std::move(all)` casts to rvalue-reference.
    // `std::get` moves only the requested element.

    std::vector<Model> pages = fromData<Model>(std::get<std::vector<WebPageData>>(std::move(all)));
    std::unordered_map<QnUuid, std::vector<ResourceParamData>> parameters =
        toParameterMap(std::get<std::vector<ResourceParamWithRefData>>(std::move(all)));

    for (auto& p: pages)
    {
        if (const auto f = parameters.find(p.id); f != parameters.cend())
        {
            for (ResourceParamData& r: f->second)
                static_cast<ResourceWithParameters&>(p).setFromParameter(r);

            parameters.erase(f);
        }

        // extracting parameters to fields
        if constexpr (std::is_same_v<Model, WebPageModelV3>)
        {
            if (const auto it = p.parameters.find(kCertificateCheckPropertyName);
                it != p.parameters.cend())
            {
                p.certificateCheck = static_cast<bool>(it->second.toInt());
                p.parameters.erase(it);
            }

            if (const auto it = p.parameters.find(kProxyDomainAllowListPropertyName);
                it != p.parameters.cend())
            {
                QJson::deserialize(it->second, &p.proxyDomainAllowList);
                p.parameters.erase(it);
            }
        }
    }

    return pages;
}

} // namespace

WebPageModelV1::DbUpdateTypes WebPageModelV1::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<WebPageModelV1> WebPageModelV1::fromDbTypes(DbListTypes all)
{
    return api::fromDbTypes<WebPageModelV1>(std::move(all));
}

WebPageModelV3::DbUpdateTypes WebPageModelV3::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<WebPageModelV3> WebPageModelV3::fromDbTypes(DbListTypes all)
{
    return api::fromDbTypes<WebPageModelV3>(std::move(all));
}

} // namespace nx::vms::api
