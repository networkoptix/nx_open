// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/coro/task.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>

namespace nx::vms::client::core {

struct ChannelPartner
{
    nx::Uuid id;
    std::string name;
    int partnerCount = 0;

    bool operator==(const ChannelPartner&) const = default;
};
NX_REFLECTION_INSTRUMENT(ChannelPartner, (id)(name)(partnerCount))

struct ChannelPartnerList
{
    int count = 0;
    std::string next;
    std::string previous;
    std::vector<ChannelPartner> results;

    bool operator==(const ChannelPartnerList&) const = default;
};
NX_REFLECTION_INSTRUMENT(ChannelPartnerList, (count)(next)(previous)(results));

struct Organization
{
    nx::Uuid id;
    std::string name;
    int systemCount = 0;
    nx::Uuid channelPartner;
    bool operator==(const Organization& other) const = default;
};
NX_REFLECTION_INSTRUMENT(Organization, (id)(name)(systemCount)(channelPartner))

struct OrganizationList
{
    int count = 0;
    std::string next;
    std::string previous;
    std::vector<Organization> results;
};
NX_REFLECTION_INSTRUMENT(OrganizationList, (count)(next)(previous)(results));

struct GroupStructure
{
    nx::Uuid id;
    std::string name;
    nx::Uuid parentId;
    std::vector<GroupStructure> children;
    int systemCount = 0;

    bool operator==(const GroupStructure& other) const = default;
};
NX_REFLECTION_INSTRUMENT(GroupStructure, (id)(name)(parentId)(children)(systemCount));

struct SystemInOrganization
{
    nx::Uuid systemId;
    std::string name;
    std::string state;
    nx::Uuid groupId;

    bool operator==(const SystemInOrganization& other) const = default;
};
NX_REFLECTION_INSTRUMENT(SystemInOrganization, (systemId)(name)(state)(groupId));

struct SystemList
{
    int count = 0;
    std::string next;
    std::string previous;
    std::vector<SystemInOrganization> results;
};
NX_REFLECTION_INSTRUMENT(SystemList, (count)(next)(previous)(results));

template<typename T>
using CloudResult = nx::utils::expected<T, nx::cloud::db::api::ResultCode>;

// Makes a GET request to the cloud API. Resumes on the main thread.
template<typename T>
nx::coro::Task<CloudResult<T>> cloudGet(
    CloudStatusWatcher* statusWatcher,
    const std::string& path,
    const nx::utils::UrlQuery& query = {})
{
    struct Awaiter
    {
        Awaiter(
            nx::cloud::db::client::Connection* connection,
            const std::string& path,
            const nx::utils::UrlQuery& query)
            :
            m_connection(connection),
            m_path(path),
            m_query(query)
        {
        }

        bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            auto g = nx::utils::ScopeGuard(
                [h]()
                {
                    QMetaObject::invokeMethod(
                        qApp,
                        [h]() mutable { h.resume(); },
                        Qt::QueuedConnection);
                });

            m_connection->requestsExecutor()->makeAsyncCall<T>(
                nx::network::http::Method::get,
                m_path,
                m_query,
                [this, g = std::move(g)](nx::cloud::db::api::ResultCode code, auto data) mutable
                {
                    if (code != nx::cloud::db::api::ResultCode::ok)
                        m_result = nx::utils::unexpected(code);
                    else
                        m_result = std::move(data);

                    // Resume the coroutine at the exit of the current scope.
                    auto runAtScopeExit = std::move(g);
                });
        }

        CloudResult<T> await_resume()
        {
            if (!m_result)
                return nx::utils::unexpected(nx::cloud::db::api::ResultCode::unknownError);

            auto result = std::move(*m_result);

            return result;
        }

        nx::cloud::db::client::Connection* const m_connection;
        std::string m_path;
        nx::utils::UrlQuery m_query;
        std::optional<CloudResult<T>> m_result;
    };

    if (!statusWatcher->cloudConnection() || statusWatcher->status() != CloudStatusWatcher::Online)
        co_return nx::utils::unexpected(nx::cloud::db::api::ResultCode::networkError);

    co_return co_await Awaiter(statusWatcher->cloudConnection(), path, query);
}

} // namespace nx::vms::client::core
