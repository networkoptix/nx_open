// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/helpers/string.h>

#include "device_agent.h"

namespace nx::vms_server_plugins::cloud_storage::sample {

Engine::Engine(
    const nx::sdk::cloud_storage::IAsyncOperationHandler*,
    const std::string& /*integrationId*/)
{
}

void Engine::startAsyncTasks()
{
}

void Engine::stopAsyncTasks()
{
}

void Engine::doObtainDeviceAgent(
    nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>* outResult,
    const nx::sdk::IDeviceInfo*)
{
    *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented,
        new nx::sdk::String("Not implemented")));
}

void Engine::doQueryBookmarks(
    const char* /*filter*/,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented,
        new nx::sdk::String("Not implemented")));
}

nx::sdk::ErrorCode Engine::deleteBookmark(const char* /*bookmarkId*/)
{
    return nx::sdk::ErrorCode::notImplemented;
}

void Engine::doQueryMotionTimePeriods(
    const char* /*filter*/,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented,
        new nx::sdk::String("Not implemented")));
}

void Engine::doQueryAnalytics(
    const char* /*filter*/,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::internalError,
        new nx::sdk::String("Not implemented")));
}

void Engine::doQueryAnalyticsTimePeriods(
    const char* /*filter*/,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented,
        new nx::sdk::String("Not implemented")));
}

nx::sdk::ErrorCode Engine::saveMetadata(
    const char* /*deviceId*/,
    nx::sdk::cloud_storage::MetadataType /*type*/,
    const char* /*data*/)
{
    return nx::sdk::ErrorCode::notImplemented;
}

bool Engine::isOnline() const
{
    return true;
}

nx::sdk::ErrorCode Engine::storageSpace(
    nx::sdk::cloud_storage::StorageSpace* /*storageSpace*/) const
{
    return nx::sdk::ErrorCode::notImplemented;
}

void Engine::doFetchTrackImage(
    const char* /*objectTrackId*/,
    nx::sdk::cloud_storage::TrackImageType /*type*/,
    nx::sdk::Result<nx::sdk::IString*>* outResult) const
{
    *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

} // nx::vms_server_plugins::cloud_storage::sample
