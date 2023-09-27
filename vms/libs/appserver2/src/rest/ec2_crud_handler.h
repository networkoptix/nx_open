// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <core/resource/resource_type.h>
#include <database/db_manager.h>
#include <nx/network/rest/params.h>
#include <nx/network/rest/request.h>
#include <nx/vms/ec2/base_ec2_connection.h> // TODO: Add direct includes instead.
#include <transaction/transaction.h>

namespace ec2 {

template<
    typename Filter,
    typename Model,
    typename DeleteInput,
    typename QueryProcessor,
    ApiCommand::Value DeleteCommand
>
class CrudHandler
{
public:
    CrudHandler(QueryProcessor* queryProcessor): m_queryProcessor(queryProcessor) {}

    std::vector<Model> read(Filter filter, const nx::network::rest::Request& request)
    {
        auto processor = m_queryProcessor->getAccess(request.userSession);
        validateType(processor, filter);

        auto query =
            [id = std::move(filter.getId()), &processor](auto&& x)
            {
                using Arg = std::decay_t<decltype(x)>;
                using Output = std::conditional_t<IsVector<Arg>::value, Arg, std::vector<Arg>>;
                auto future = processor.template processQueryAsync<decltype(id), Output>(
                    CrudHandler::template getReadCommand<Output>(),
                    std::move(id),
                    std::make_pair<Result, Output>);
                future.waitForFinished();
                auto [result, output] = future.resultAt(0);
                if (!result)
                    CrudHandler::throwError(std::move(result));
                return output;
            };
        if constexpr (fromDbTypesExists<Model>::value)
        {
            return Model::fromDbTypes(
                callQueryFunc(typename Model::DbReadTypes(), std::move(query)));
        }
        else
        {
            return query(Model());
        }
    }

    void delete_(DeleteInput id, const nx::network::rest::Request& request)
    {
        auto processor = m_queryProcessor->getAccess(request.userSession);
        validateType(processor, id);

        std::promise<Result> promise;
        processor.processUpdateAsync(
            DeleteCommand,
            std::move(id),
            [&promise](Result result) { promise.set_value(std::move(result)); });
        if (Result result = promise.get_future().get(); !result)
            throwError(std::move(result));
    }

    void create(Model data, const nx::network::rest::Request& request)
    {
        update(std::move(data), request);
    }

    void update(Model data, const nx::network::rest::Request& request)
    {
        std::promise<Result> promise;
        auto processor = m_queryProcessor->getAccess(request.userSession);
        if constexpr (toDbTypesExists<Model>::value)
        {
            auto id = data.getId();
            auto items = std::move(data).toDbTypes();
            using IgnoredDbType = std::decay_t<decltype(std::get<0>(items))>;
            callUpdateFunc(items,
                [&processor](const auto& x)
                {
                    if (const auto r = validateType(processor, x); !r)
                        return r;
                    return validateResourceTypeId(x);
                });

            auto update =
                [id = std::move(id)](auto&& x, auto& copy, auto* list)
                {
                    using DbType = std::decay_t<decltype(x)>;
                    Result result;
                    assertModelToDbTypesProducedValidResult<IgnoredDbType, DbType>(x, id);
                    if constexpr (IsVector<DbType>::value)
                    {
                        const auto command = CrudHandler::template getUpdateCommand<std::decay_t<decltype(x[0])>>();
                        result = copy.template processMultiUpdateSync<DbType>(
                            parentUpdateCommand(command),
                            command,
                            TransactionType::Regular,
                            std::move(x),
                            list);
                    }
                    else
                    {
                        result = copy.processUpdateSync(
                            CrudHandler::template getUpdateCommand<DbType>(), std::move(x), list);
                    }
                    return result;
                };
            processor.processCustomUpdateAsync(
                ApiCommand::CompositeSave,
                [&promise](Result result) { promise.set_value(std::move(result)); },
                [update = std::move(update), items = std::move(items)](auto& copy, auto* list)
                {
                    return callUpdateFunc(
                        std::move(items),
                        [update = std::move(update), &copy, list](auto&& x) mutable
                        {
                            if constexpr (IsOptional<std::decay_t<decltype(x)>>::value)
                                return x ? update(std::move(*x), copy, list) : Result();
                            else
                                return update(std::move(x), copy, list);
                        });
                });
        }
        else
        {
            validateType(processor, data);
            processor.processUpdateAsync(
                CrudHandler::template getUpdateCommand<Model>(),
                data,
                [&promise](Result result) { promise.set_value(std::move(result)); });
        }
        if (Result result = promise.get_future().get(); !result)
            throwError(std::move(result));
    }

protected:
    QueryProcessor* const m_queryProcessor;

private:
#define MEMBER_CHECKER(member) \
    template<typename T, typename = std::void_t<>> \
    struct member##Exists: std::false_type \
    { \
    }; \
    template<typename T> \
    struct member##Exists<T, std::void_t<decltype(&T::member)>>: std::true_type \
    { \
    }

    MEMBER_CHECKER(fromDbTypes);
    MEMBER_CHECKER(toDbTypes);
    MEMBER_CHECKER(kResourceTypeId);
    MEMBER_CHECKER(id);
#undef MEMBER_CHECKER

    template<typename>
    struct IsVector: std::false_type
    {
    };

    template<typename T, typename A>
    struct IsVector<std::vector<T, A>>: std::true_type
    {
    };

    template<typename>
    struct IsOptional: std::false_type
    {
    };

    template<typename T>
    struct IsOptional<std::optional<T>>: std::true_type
    {
    };

    template<typename F, typename... Args, size_t... Is>
    static auto callQueryFuncImpl(const std::tuple<Args...>& t, F&& f, std::index_sequence<Is...>)
    {
        return std::make_tuple(f(std::get<Is>(t))...);
    }

    template<typename F, typename... Args>
    static auto callQueryFunc(const std::tuple<Args...>& t, F&& f)
    {
        return callQueryFuncImpl(
            t, std::forward<F>(f), std::make_index_sequence<sizeof...(Args)>{});
    }

    template<typename F, typename... Args, size_t... Is>
    static Result callUpdateFuncImpl(
        const std::tuple<Args...>& t, F&& f, std::index_sequence<Is...>)
    {
        Result result;
        (... && (result = f(std::get<Is>(t))));
        return result;
    }

    template<typename F, typename... Args>
    static Result callUpdateFunc(const std::tuple<Args...>& t, F&& f)
    {
        return callUpdateFuncImpl(
            t, std::forward<F>(f), std::make_index_sequence<sizeof...(Args)>{});
    }

    static void throwError(Result r)
    {
        using Exception = nx::network::rest::Exception;
        switch (r.error)
        {
            case ErrorCode::forbidden:
                throw Exception::forbidden(std::move(r.message));
            case ErrorCode::badRequest:
            case ErrorCode::dbError:
                throw Exception::badRequest(std::move(r.message));
            case ErrorCode::notFound:
                throw Exception::notFound(std::move(r.message));
            default:
                NX_ASSERT(false, "Unexpected code: %1", r.toString());
                throw Exception::internalServerError(NX_FMT("Unexpected code: %1", r.toString()));
        }
    }

    template<typename IgnoredDbType, typename DbType, typename Id>
    static void assertModelToDbTypesProducedValidResult(const DbType& value, const Id& id)
    {
        if constexpr (!std::is_same_v<DbType, IgnoredDbType>
            && !std::is_same_v<DbType, nx::vms::api::ResourceStatusData>)
        {
            if constexpr (IsVector<DbType>::value)
            {
                NX_ASSERT(std::find_if(value.begin(), value.end(),
                    [&id](const auto& item)
                    {
                        return item.checkResourceExists == nx::vms::api::CheckResourceExists::yes
                            || item.getId() != id;
                    }) == value.end(),
                    "%1 toDbTypes() must fill %2 with resource id %3 and checkResourceExists != yes",
                    typeid(Model), typeid(DbType), id);
            }
            else
            {
                NX_ASSERT(value.checkResourceExists != nx::vms::api::CheckResourceExists::yes
                        && value.getId() == id,
                    "%1 toDbTypes() must fill %2 with resource id %3 and checkResourceExists != yes",
                    typeid(Model), typeid(DbType), id);
            }
        }
    }

    template<typename T>
    static constexpr ApiCommand::Value getReadCommand()
    {
        static_assert(
            std::is_same_v<T, nx::vms::api::StoredFileDataList> ||
            std::is_same_v<T, nx::vms::api::UserDataList> ||
            std::is_same_v<T, nx::vms::api::UserGroupDataList> ||
            std::is_same_v<T, nx::vms::api::LayoutDataList> ||
            std::is_same_v<T, nx::vms::api::ShowreelDataList> ||
            std::is_same_v<T, nx::vms::api::LookupListDataList> ||
            std::is_same_v<T, nx::vms::api::VideowallDataList> ||
            std::is_same_v<T, nx::vms::api::LicenseDataList> ||
            std::is_same_v<T, nx::vms::api::CameraDataList> ||
            std::is_same_v<T, nx::vms::api::CameraAttributesDataList> ||
            std::is_same_v<T, nx::vms::api::WebPageDataList> ||
            std::is_same_v<T, nx::vms::api::StorageDataList> ||
            std::is_same_v<T, nx::vms::api::MediaServerDataList> ||
            std::is_same_v<T, nx::vms::api::MediaServerUserAttributesDataList> ||
            std::is_same_v<T, nx::vms::api::ResourceStatusDataList> ||
            std::is_same_v<T, nx::vms::api::ResourceParamWithRefDataList> ||
            std::is_same_v<T, nx::vms::api::AnalyticsPluginDataList> ||
            std::is_same_v<T, nx::vms::api::AnalyticsEngineDataList> ||
            std::is_same_v<T, nx::vms::api::rules::RuleList>);
        if constexpr(std::is_same_v<T, nx::vms::api::StoredFileDataList>)
            return ApiCommand::Value::getStoredFiles;
        else if constexpr(std::is_same_v<T, nx::vms::api::UserDataList>)
            return ApiCommand::Value::getUsers;
        else if constexpr(std::is_same_v<T, nx::vms::api::UserGroupDataList>)
            return ApiCommand::Value::getUserGroups;
        else if constexpr(std::is_same_v<T, nx::vms::api::LayoutDataList>)
            return ApiCommand::Value::getLayouts;
        else if constexpr(std::is_same_v<T, nx::vms::api::ShowreelDataList>)
            return ApiCommand::Value::getShowreels;
        else if constexpr (std::is_same_v<T, nx::vms::api::LookupListDataList>)
            return ApiCommand::Value::getLookupLists;
        else if constexpr(std::is_same_v<T, nx::vms::api::VideowallDataList>)
            return ApiCommand::Value::getVideowalls;
        else if constexpr(std::is_same_v<T, nx::vms::api::LicenseDataList>)
            return ApiCommand::Value::getLicenses;
        else if constexpr(std::is_same_v<T, nx::vms::api::CameraDataList>)
            return ApiCommand::Value::getCameras;
        else if constexpr(std::is_same_v<T, nx::vms::api::CameraAttributesDataList>)
            return ApiCommand::Value::getCameraUserAttributesList;
        else if constexpr(std::is_same_v<T, nx::vms::api::WebPageDataList>)
            return ApiCommand::Value::getWebPages;
        else if constexpr(std::is_same_v<T, nx::vms::api::StorageDataList>)
            return ApiCommand::Value::getStorages;
        else if constexpr(std::is_same_v<T, nx::vms::api::MediaServerDataList>)
            return ApiCommand::Value::getMediaServers;
        else if constexpr(std::is_same_v<T, nx::vms::api::MediaServerUserAttributesDataList>)
            return ApiCommand::Value::getMediaServerUserAttributesList;
        else if constexpr(std::is_same_v<T, nx::vms::api::ResourceStatusDataList>)
            return ApiCommand::Value::getStatusList;
        else if constexpr(std::is_same_v<T, nx::vms::api::ResourceParamWithRefDataList>)
            return ApiCommand::Value::getResourceParams;
        else if constexpr(std::is_same_v<T, nx::vms::api::AnalyticsPluginDataList>)
            return ApiCommand::Value::getAnalyticsPlugins;
        else if constexpr(std::is_same_v<T, nx::vms::api::AnalyticsEngineDataList>)
            return ApiCommand::Value::getAnalyticsEngines;
        else if constexpr (std::is_same_v<T, nx::vms::api::rules::RuleList>)
            return ApiCommand::Value::getVmsRules;
        else
            return ApiCommand::NotDefined;
    }

    static constexpr ApiCommand::Value parentUpdateCommand(const ApiCommand::Value command)
    {
        switch (command)
        {
            case ApiCommand::Value::saveCamera:
                return ApiCommand::Value::saveCameras;
            case ApiCommand::Value::saveCameraUserAttributes:
                return ApiCommand::Value::saveCameraUserAttributesList;
            case ApiCommand::Value::saveMediaServerUserAttributes:
                return ApiCommand::Value::saveMediaServerUserAttributesList;
            case ApiCommand::Value::setResourceParam:
                return ApiCommand::Value::setResourceParams;
            case ApiCommand::Value::saveLayout:
                return ApiCommand::Value::saveLayouts;
            case ApiCommand::Value::addLicense:
                return ApiCommand::Value::addLicenses;
            default:
                return ApiCommand::NotDefined;
        }
    }

    template<typename T>
    static constexpr ApiCommand::Value getUpdateCommand()
    {
        static_assert(
            std::is_same_v<T, nx::vms::api::StoredFileData> ||
            std::is_same_v<T, nx::vms::api::UserGroupData> ||
            std::is_same_v<T, nx::vms::api::UserDataEx> ||
            std::is_same_v<T, nx::vms::api::LayoutData> ||
            std::is_same_v<T, nx::vms::api::ShowreelData> ||
            std::is_same_v<T, nx::vms::api::LookupListData> ||
            std::is_same_v<T, nx::vms::api::VideowallData> ||
            std::is_same_v<T, nx::vms::api::LicenseData> ||
            std::is_same_v<T, nx::vms::api::CameraData> ||
            std::is_same_v<T, nx::vms::api::CameraAttributesData> ||
            std::is_same_v<T, nx::vms::api::WebPageData> ||
            std::is_same_v<T, nx::vms::api::StorageData> ||
            std::is_same_v<T, nx::vms::api::MediaServerData> ||
            std::is_same_v<T, nx::vms::api::MediaServerUserAttributesData> ||
            std::is_same_v<T, nx::vms::api::ResourceStatusData> ||
            std::is_same_v<T, nx::vms::api::ResourceParamWithRefData> ||
            std::is_same_v<T, nx::vms::api::AnalyticsPluginData> ||
            std::is_same_v<T, nx::vms::api::AnalyticsEngineData> ||
            std::is_same_v<T, nx::vms::api::rules::Rule>);
        if constexpr(std::is_same_v<T, nx::vms::api::StoredFileData>)
            return ApiCommand::Value::updateStoredFile;
        else if constexpr (std::is_same_v<T, nx::vms::api::UserDataEx>)
            return ApiCommand::Value::saveUser;
        else if constexpr (std::is_same_v<T, nx::vms::api::UserGroupData>)
            return ApiCommand::Value::saveUserGroup;
        else if constexpr (std::is_same_v<T, nx::vms::api::LayoutData>)
            return ApiCommand::Value::saveLayout;
        else if constexpr (std::is_same_v<T, nx::vms::api::ShowreelData>)
            return ApiCommand::Value::saveShowreel;
        else if constexpr (std::is_same_v<T, nx::vms::api::LookupListData>)
            return ApiCommand::Value::saveLookupList;
        else if constexpr (std::is_same_v<T, nx::vms::api::VideowallData>)
            return ApiCommand::Value::saveVideowall;
        else if constexpr (std::is_same_v<T, nx::vms::api::LicenseData>)
            return ApiCommand::Value::addLicense;
        else if constexpr (std::is_same_v<T, nx::vms::api::CameraData>)
            return ApiCommand::Value::saveCamera;
        else if constexpr (std::is_same_v<T, nx::vms::api::CameraAttributesData>)
            return ApiCommand::Value::saveCameraUserAttributes;
        else if constexpr (std::is_same_v<T, nx::vms::api::WebPageData>)
            return ApiCommand::Value::saveWebPage;
        else if constexpr (std::is_same_v<T, nx::vms::api::StorageData>)
            return ApiCommand::Value::saveStorage;
        else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerData>)
            return ApiCommand::Value::saveMediaServer;
        else if constexpr (std::is_same_v<T, nx::vms::api::MediaServerUserAttributesData>)
            return ApiCommand::Value::saveMediaServerUserAttributes;
        else if constexpr (std::is_same_v<T, nx::vms::api::ResourceStatusData>)
            return ApiCommand::Value::setResourceStatus;
        else if constexpr (std::is_same_v<T, nx::vms::api::ResourceParamWithRefData>)
            return ApiCommand::Value::setResourceParam;
        else if constexpr (std::is_same_v<T, nx::vms::api::AnalyticsPluginData>)
            return ApiCommand::Value::saveAnalyticsPlugin;
        else if constexpr (std::is_same_v<T, nx::vms::api::ResourceParamWithRefData>)
            return ApiCommand::Value::saveAnalyticsEngine;
        else if constexpr (std::is_same_v<T, nx::vms::api::rules::Rule>)
            return ApiCommand::Value::saveVmsRule;
        else
            return ApiCommand::NotDefined;
    }

    template<typename T>
    static Result validateResourceTypeId(const T& data)
    {
        if constexpr (kResourceTypeIdExists<T>::value)
        {
            if (data.typeId != T::kResourceTypeId)
            {
                throw nx::network::rest::Exception::internalServerError(
                    "Parameter typeId doesn't match its resource data structure");
            }
        }

        return ErrorCode::ok;
    }

    template<typename T>
    static Result validateType(const auto& processor, const T& data)
    {
        if constexpr (idExists<T>::value)
        {
            const auto& id = data.id;
            if constexpr (std::is_same_v<std::decay_t<decltype(id)>, QnUuid>)
            {
                ec2::ApiObjectType requiredType = ApiObject_NotDefined;
                switch (DeleteCommand)
                {
                    case ApiCommand::removeMediaServer:
                        requiredType = ApiObject_Server;
                        break;
                    case ApiCommand::removeCamera:
                        requiredType = ApiObject_Camera;
                        break;
                    case ApiCommand::removeUser:
                        requiredType = ApiObject_User;
                        break;
                    case ApiCommand::removeLayout:
                        requiredType = ApiObject_Layout;
                        break;
                    case ApiCommand::removeVideowall:
                        requiredType = ApiObject_Videowall;
                        break;
                    case ApiCommand::removeStorage:
                        requiredType = ApiObject_Storage;
                        break;
                    case ApiCommand::removeWebPage:
                        requiredType = ApiObject_WebPage;
                        break;
                    case ApiCommand::removeAnalyticsPlugin:
                        requiredType = ApiObject_AnalyticsPlugin;
                        break;
                    case ApiCommand::removeAnalyticsEngine:
                        requiredType = ApiObject_AnalyticsEngine;
                        break;
                    default:
                        return ErrorCode::ok;
                }

                const auto objectType = processor.getObjectType(id);
                if (objectType == ApiObject_NotDefined || objectType == requiredType)
                    return ErrorCode::ok;

                throw nx::network::rest::Exception::notFound();
            }
        }
        else
        {
            return ErrorCode::ok;
        }
    }
};

} // namespace ec2
