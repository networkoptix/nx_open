// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nx/sdk/cloud_storage/i_archive_update_handler.h>
#include <nx/sdk/cloud_storage/i_engine.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_utility_provider.h>

#include "data_manager.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

class Plugin;

class Engine: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IEngine>
{
public:
    Engine(
        const nx::sdk::cloud_storage::IArchiveUpdateHandler* deviceManagerHandler,
        const std::shared_ptr<DataManager>& dataManager,
        const std::string& pluginId);

    virtual ~Engine() override;
    virtual void startNotifications() override;
    virtual void stopNotifications() override;
    virtual nx::sdk::ErrorCode saveMetadata(
        const char* deviceId,
        int64_t timeStampUs,
        nx::sdk::cloud_storage::MetadataType type,
        const char* data) override;

    virtual bool isOnline() const override;
    virtual nx::sdk::ErrorCode storageSpace(
        nx::sdk::cloud_storage::StorageSpace* storageSpace) const override;

    virtual nx::sdk::ErrorCode saveTrackImage(
        const char* data,
        nx::sdk::cloud_storage::TrackImageType type) override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual void doQueryMotionTimePeriods(
        const char* filter,
        nx::sdk::Result<nx::sdk::IString*>* outResult) override;

    virtual void doQueryAnalytics(
        const char* filter,
        nx::sdk::Result<nx::sdk::IString*>* outResult) override;

    virtual void doQueryAnalyticsTimePeriods(
        const char* filter,
        nx::sdk::Result<nx::sdk::IString*>* outResult) override;

        virtual void doQueryBookmarks(
        const char* filter,
        nx::sdk::Result<nx::sdk::IString*>* outResult) override;

    virtual nx::sdk::ErrorCode deleteBookmark(const char* bookmarkId) override;
    virtual void doFetchTrackImage(
        const char* objectTrackId,
        nx::sdk::cloud_storage::TrackImageType type,
        nx::sdk::Result<nx::sdk::IString*>* outResult) const override;

private:
    nx::sdk::Ptr<const nx::sdk::cloud_storage::IArchiveUpdateHandler> m_handler;
    std::shared_ptr<DataManager> m_dataManager;
    std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>> m_deviceAgents;
    std::string m_pluginId;
    std::thread m_worker;
    mutable std::mutex m_mutex;
    bool m_needToStop = false;
    std::optional<std::chrono::system_clock::time_point> m_lastReportTimePoint;
    std::optional<std::chrono::system_clock::time_point> m_lastScanTimePoint;

    nx::sdk::cloud_storage::IDeviceAgent* findDeviceAgentById(
        const std::string& id,
        const std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>>& deviceAgents);
};

} // nx::vms_server_plugins::cloud_storage::stub
