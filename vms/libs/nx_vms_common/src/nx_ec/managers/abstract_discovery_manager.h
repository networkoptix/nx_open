// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/discovery_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractDiscoveryNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void peerDiscoveryRequested(const nx::utils::Url& url);
    void discoveryInformationChanged(const nx::vms::api::DiscoveryData& data, bool addInformation);
    void discoveredServerChanged(const nx::vms::api::DiscoveredServerData& discoveredServer);
    void gotInitialDiscoveredServers(const nx::vms::api::DiscoveredServerDataList& discoveredServers);
};

class NX_VMS_COMMON_API AbstractDiscoveryManager
{
public:
    virtual ~AbstractDiscoveryManager() = default;

    virtual int discoverPeer(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode discoverPeerSync(const nx::Uuid& id, const nx::utils::Url& url);

    virtual int addDiscoveryInformation(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode addDiscoveryInformationSync(
        const nx::Uuid& id, const nx::utils::Url& url, bool ignore);

    virtual int removeDiscoveryInformation(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeDiscoveryInformationSync(
        const nx::Uuid& id, const nx::utils::Url& url, bool ignore);

    virtual int getDiscoveryData(
        Handler<nx::vms::api::DiscoveryDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getDiscoveryDataSync(nx::vms::api::DiscoveryDataList* outDataList);
};

} // namespace ec2
