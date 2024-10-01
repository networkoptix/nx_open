// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_archive_update_handler.h>
#include <nx/sdk/cloud_storage/i_engine.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_utility_provider.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

class Engine: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IEngine>
{
public:
    Engine(
        const nx::sdk::cloud_storage::IArchiveUpdateHandler* deviceManagerHandler,
        const std::string& integrationId);

    virtual void startNotifications() override;
    virtual void stopNotifications() override;

    virtual void doQueryBookmarks(
        const char* filter,
        nx::sdk::Result<nx::sdk::IString*>* outResult) override;

    virtual nx::sdk::ErrorCode deleteBookmark(const char* bookmarkId) override;
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

    virtual void doFetchTrackImage(
        const char* objectTrackId,
        nx::sdk::cloud_storage::TrackImageType type,
        nx::sdk::Result<nx::sdk::IString*>* outResult) const override;
};

} // nx::vms_server_plugins::cloud_storage::sample
