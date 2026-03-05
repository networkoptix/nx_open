// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_factory.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/server_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/api/data/webpage_model.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::client::core {

namespace details {

template<typename Data, typename Functor>
auto invokeOnOptional(Data&& data, Functor&& functor)
{
    if constexpr (nx::traits::is<std::optional, std::decay_t<decltype(data)>>())
        return functor(std::move(*data));
    else
        return functor(std::move(data));
}

template<typename DbData>
void updateCommonData(const QnResourcePtr& resource, DbData&& items)
{
    auto update =
        [resource = resource](auto&& data)
        {
            using DbType = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<nx::vms::api::ResourceStatusData, DbType>)
            {
                resource->setStatus(data.status);
            }
            else if constexpr (std::is_same_v<nx::vms::api::ResourceParamWithRefDataList, DbType>)
            {
                for (const auto& param: data)
                    resource->setProperty(param.name, param.value, /*markDirty*/ false);
            }
        };

    std::apply([&update](auto&& ...x){(..., invokeOnOptional(x, update));}, std::move(items));
}

} // namespace details

template<typename Model>
struct ModelTraits
{
    using ResourceType = QnResource;
};

template<>
struct ModelTraits<nx::vms::api::WebPageModelV3>
{
    using ResourceType = QnWebPageResource;
};

template<typename Model>
struct ResourceTraits
{
    using ResourceType = ModelTraits<Model>::ResourceType;
    using DbTypes = Model::DbUpdateTypes;

    static DbTypes toDbTypes(Model&& model)
    {
        return std::move(model).toDbTypes();
    }

    static QnResourcePtr create(QnResourceFactory* factory, const DbTypes& items)
    {
        auto res = factory->createResource(
            std::decay_t<decltype(std::get<0>(items))>::kResourceTypeId, /*params*/ {})
                .template dynamicCast<ResourceType>();

        if (res)
            ec2::fromApiToResource(std::get<0>(items), res);

        return res;
    }

    static void update(const QnResourcePtr& resource, DbTypes&& items, bool /*initial*/)
    {
        details::updateCommonData(resource, std::move(items));
    }
};

template<>
struct ResourceTraits<nx::vms::api::DeviceModelV4>
{
    using ResourceType = QnVirtualCameraResource;
    using Model = nx::vms::api::DeviceModelV4;
    using DbTypes = Model::DbUpdateTypes;

    static auto toDbTypes(Model&& model)
    {
        return std::move(model).toDbTypes();
    }

    static auto create(QnResourceFactory* factory, const DbTypes& items)
    {
        const auto& data = std::get<0>(items);

        auto result = factory->createResource(data.typeId, {}).template dynamicCast<ResourceType>();
        if (result)
            ec2::fromApiToResource(data, result);

        return result;
    }

    static void update(const QnResourcePtr& resource, DbTypes&& items, bool initial)
    {
        auto device = resource.staticCast<ResourceType>();
        if (const auto& attrs = std::get<1>(items))
        {
            if (initial)
                device->setUserAttributes(*attrs);
            else
                device->setUserAttributesAndNotify(*attrs);
        }

        details::updateCommonData(resource, std::move(items));
    }
};

template<>
struct ResourceTraits<nx::vms::api::ServerModelV4>
{
    using Model = nx::vms::api::ServerModelV4;
    using ResourceType = QnMediaServerResource;

    using DbTypes = decltype(
        std::tuple_cat(Model{}.toDbTypes(), std::make_tuple(nx::vms::api::ResourceStatusData{})));

    static DbTypes toDbTypes(Model&& model)
    {
        auto status = nx::vms::api::ResourceStatusData(model.id, model.status);

        return std::tuple_cat(std::move(model).toDbTypes(), std::make_tuple(std::move(status)));
    }

    static QnResourcePtr create(QnResourceFactory* factory, const DbTypes& items)
    {
        const auto& data = std::get<0>(items);

        auto result = factory->createServer();
        if (result)
            ec2::fromApiToResource(data, result);

        return result;
    }

    static void update(const QnResourcePtr& resource, DbTypes&& items, bool initial)
    {
        auto server = resource.staticCast<ResourceType>();
        if (const auto& attrs = std::get<1>(items))
        {
            if (initial)
                server->setUserAttributes(*attrs);
            else
                server->setUserAttributesAndNotify(*attrs);
        }

        details::updateCommonData(resource, std::move(items));
    }
};

} // namespace nx::vms::client::core
