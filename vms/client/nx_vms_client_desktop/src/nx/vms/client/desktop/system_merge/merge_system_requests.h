// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/network/rest/result.h>
#include <nx/network/ssl/helpers.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/merge_status_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/system_merge_data.h>

namespace nx::network::http { class Credentials; }

namespace rest {

template<typename Data>
using ErrorOrData = std::variant<nx::network::rest::Result, Data>;

} // namespace rest

namespace nx::vms::client::desktop {

/**
 * Helper class for sending requests related to system merge process.
 * Implements HTTP CONNECT proxifying via current server.
 */
class MergeSystemRequestsManager
{
    template<typename Data>
    using Callback = std::function<void(const rest::ErrorOrData<Data>& data)>;

public:
    MergeSystemRequestsManager(QObject* parent, const std::string& locale);
    ~MergeSystemRequestsManager();

    void getTargetInfo(
        QnMediaServerResourcePtr proxy,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        const nx::network::SocketAddress& target,
        nx::network::ssl::AdapterFunc targetAdapterFunc,
        Callback<nx::vms::api::ServerInformation> callback);

    void createTargetSession(
        QnMediaServerResourcePtr proxy,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        const nx::network::SocketAddress& target,
        nx::network::ssl::AdapterFunc targetAdapterFunc,
        const nx::vms::api::LoginSessionRequest& request,
        Callback<nx::vms::api::LoginSession> callback);

    void getTargetLicenses(
        QnMediaServerResourcePtr proxy,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        const nx::network::SocketAddress& target,
        const std::string& authToken,
        nx::network::ssl::AdapterFunc targetAdapterFunc,
        Callback<nx::vms::api::LicenseDataList> callback);

    void mergeSystem(
        QnMediaServerResourcePtr proxy,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        const nx::network::http::Credentials& credentials,
        const nx::vms::api::SystemMergeData& request,
        Callback<nx::vms::api::MergeStatusReply> callback);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
