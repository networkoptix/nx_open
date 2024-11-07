// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <optional>

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/device_archive.h>
#include <nx/sdk/cloud_storage/helpers/time_periods.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/to_string.h>

#include "device_agent.h"
#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

using namespace std::chrono;

static constexpr seconds kReportPeriod = 5s;

Engine::~Engine()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_needToStop)
            return;
    }

    NX_OUTPUT << __func__ << "Warning: worker thread has not been stopped";
    stopAsyncTasks();
}

Engine::Engine(
    const nx::sdk::cloud_storage::IAsyncOperationHandler* asyncOperationHandler,
    const std::shared_ptr<DataManager>& dataManager,
    const std::string& integrationId)
    :
    m_dataManager(dataManager),
    m_integrationId(integrationId)
{
    m_handler.reset(asyncOperationHandler);
    m_handler->addRef();
    m_dataManager->setSaveHandler(
        [this](nx::sdk::cloud_storage::MetadataType type, nx::sdk::ErrorCode result)
        {
            m_handler->onSaveOperationCompleted(m_integrationId.data(), type, result);
        });
}

void Engine::stopAsyncTasks()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_needToStop)
            return;

        m_needToStop = true;
    }

    m_worker.join();
    NX_OUTPUT << __func__ << ": Worker thread stopped";
}

void Engine::startAsyncTasks()
{
    m_worker = std::thread(
        [this]()
        {
            NX_OUTPUT << "Starting worker thread";

            while (true)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_needToStop)
                {
                    NX_OUTPUT << "Stopping worker thread";
                    return;
                }

                if (m_lastReportTimePoint
                    && *m_lastReportTimePoint + kReportPeriod > system_clock::now())
                {
                    lock.unlock();
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                using namespace nx::sdk::cloud_storage;
                constexpr static auto kSafetyScanPeriod = 2min;
                std::optional<system_clock::time_point> scanStartTime = m_lastScanTimePoint
                    ? std::optional<system_clock::time_point>(std::max(
                        system_clock::time_point(0ms), *m_lastScanTimePoint - kSafetyScanPeriod))
                    : std::nullopt;

                nx::sdk::cloud_storage::CloudChunkData cloudChunkData;
                const auto archiveIndex = m_dataManager->getArchive(scanStartTime);
                for (const auto& deviceArchiveIndex: archiveIndex.deviceArchiveIndexList)
                {
                    const auto deviceId = deviceArchiveIndex.deviceDescription.deviceId();

                    auto deviceAgent = findDeviceAgentById(deviceId->data(), m_deviceAgents);
                    if (!deviceAgent)
                    {
                        deviceAgent =
                            new DeviceAgent(deviceArchiveIndex.deviceDescription, m_dataManager);
                        m_deviceAgents.push_back(nx::sdk::Ptr(deviceAgent));
                    }

                    for (const auto& entry: deviceArchiveIndex.timePeriodsPerStream)
                    {
                        NX_OUTPUT << "New time periods for device '" << *deviceId
                                  << "': " << nx::kit::Json(entry.second).dump();

                        if (!entry.second.empty()
                            && system_clock::time_point(entry.second.back().startTimestamp) > scanStartTime)
                        {
                            scanStartTime =
                                system_clock::time_point(entry.second.back().startTimestamp);
                        }

                        const auto add = nx::sdk::cloud_storage::ChunkOperation::add;
                        cloudChunkData[*deviceId][entry.first][add] = entry.second;
                    }
                }

                nx::sdk::List<nx::sdk::cloud_storage::IDeviceArchive> result;
                for (const auto& deviceIdToDeviceData: cloudChunkData)
                {
                    const auto deviceId = deviceIdToDeviceData.first;
                    auto deviceAgent = findDeviceAgentById(deviceId, m_deviceAgents);
                    result.addItem(new nx::sdk::cloud_storage::DeviceArchive(
                        deviceAgent, deviceIdToDeviceData.second));
                }

                // Real-life implementation should manage its free space by itself and
                // report data removal along with the newly added time periods here.
                m_handler->onArchiveUpdated(
                    m_integrationId.data(), nx::sdk::ErrorCode::noError, &result);

                m_lastScanTimePoint = scanStartTime;
                m_lastReportTimePoint = system_clock::now();
            }
        });
}

void Engine::doObtainDeviceAgent(
    nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>* outResult,
    const nx::sdk::IDeviceInfo* deviceInfo)
{
    const auto deviceDescription = nx::sdk::cloud_storage::DeviceDescription(deviceInfo);
    auto deviceId = deviceDescription.deviceId();
    if (!deviceId)
    {
        NX_OUTPUT << "There was an attempt to add device with NULL id. Returning NULL";
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>(nx::sdk::Error(
            nx::sdk::ErrorCode::otherError,
            new nx::sdk::String("Adding devices with NULL id is not supported")));
    }

    try
    {
        std::lock_guard lock(m_mutex);
        // We must check for the existing device first and return it if found.
        auto deviceAgent = findDeviceAgentById(*deviceId, m_deviceAgents);
        if (!deviceAgent)
        {
            deviceAgent = new DeviceAgent(deviceDescription, m_dataManager);
            NX_OUTPUT << "Device '" << *deviceId << "' has been successfully added";
            m_deviceAgents.push_back(nx::sdk::Ptr(deviceAgent));
        }
        else
        {
            NX_OUTPUT << "Device '" << *deviceId << "' already exists. Returning it";
        }

        deviceAgent->addRef();
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>(deviceAgent);
    }
    catch (const std::exception& e)
    {
        std::stringstream ss;
        ss << "Failed to save device " << *deviceId << ". Error: " << e.what();
        NX_OUTPUT << ss.str();
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IDeviceAgent*>(nx::sdk::Error(
            nx::sdk::ErrorCode::otherError,
            new nx::sdk::String(ss.str())));
    }
}

nx::sdk::cloud_storage::IDeviceAgent* Engine::findDeviceAgentById(
    const std::string& id,
    const std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>>& deviceAgents)
{
    auto resultIt = std::find_if(
        deviceAgents.cbegin(), deviceAgents.cend(),
        [&id](const auto& devicePtr) { return id == deviceId(devicePtr.get()); });

    if (resultIt == deviceAgents.cend())
        return nullptr;

    return resultIt->get();
}

void Engine::doQueryBookmarks(
    const char* filter,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    try
    {
        NX_OUTPUT << __func__ << ": Quering bookmarks with filter '" << filter << "'";
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        const auto bookmarksData =
            m_dataManager->queryBookmarks(nx::sdk::cloud_storage::BookmarkFilter(filter));
        *outResult = nx::sdk::Result<nx::sdk::IString*>(new nx::sdk::String(bookmarksData));
        NX_OUTPUT << __func__ << ": Successfully fetched some bookmarks: '" << bookmarksData << "'";
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to fetch bookmarks. Error: '" << e.what() << "'";
        *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
            nx::sdk::ErrorCode::internalError,
            new nx::sdk::String(e.what())));
    }
}

nx::sdk::ErrorCode Engine::deleteBookmark(const char* bookmarkId)
{
    try
    {
        NX_OUTPUT << __func__ << ": Deleting bookmark with id '" << bookmarkId;
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        m_dataManager->deleteBookmark(bookmarkId);
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to delete bookmark. Error: '" << e.what() << "'";
        return nx::sdk::ErrorCode::internalError;
    }

    return nx::sdk::ErrorCode::noError;
}

void Engine::doQueryMotionTimePeriods(
    const char* filter,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    try
    {
        NX_OUTPUT << __func__ << ": Quering motion with filter '" << filter;
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        const auto motionData =
            m_dataManager->queryMotion(nx::sdk::cloud_storage::MotionFilter(filter));
        *outResult = nx::sdk::Result<nx::sdk::IString*>(new nx::sdk::String(motionData));
        NX_OUTPUT << __func__ << ": Successfully fetched some moiton: '" << motionData << "'";
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to fetch motion. Error: '" << e.what() << "'";
        *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
            nx::sdk::ErrorCode::internalError,
            new nx::sdk::String(e.what())));
    }
}

void Engine::doQueryAnalytics(
    const char* filter,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    try
    {
        NX_OUTPUT << __func__ << ": Quering analytics with filter '" << filter << "'";
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        const auto analyticsData =
            m_dataManager->queryAnalytics(nx::sdk::cloud_storage::AnalyticsFilter(filter));
        *outResult = nx::sdk::Result<nx::sdk::IString*>(new nx::sdk::String(analyticsData));
        NX_OUTPUT << __func__ << ": Successfully fetched analytics data: '" << analyticsData << "'";
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to fetch analytics. Error: '" << e.what() << "'";
        *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
            nx::sdk::ErrorCode::internalError,
            new nx::sdk::String(e.what())));
    }
}

void Engine::doQueryAnalyticsTimePeriods(
    const char* filter,
    nx::sdk::Result<nx::sdk::IString*>* outResult)
{
    try
    {
        NX_OUTPUT << __func__ << ": Quering analytics periods with filter '" << filter;
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        const auto analyticsPeriodData =
            m_dataManager->queryAnalyticsPeriods(nx::sdk::cloud_storage::AnalyticsFilter(filter));
        *outResult = nx::sdk::Result<nx::sdk::IString*>(new nx::sdk::String(analyticsPeriodData));
        NX_OUTPUT << __func__ << ": Successfully fetched analytics periods: '"
            << analyticsPeriodData << "'";
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to fetch analytics periods. Error: '" << e.what() << "'";
        *outResult = nx::sdk::Result<nx::sdk::IString*>(nx::sdk::Error(
            nx::sdk::ErrorCode::internalError,
            new nx::sdk::String(e.what())));
    }
}

void Engine::flushMetadata(nx::sdk::cloud_storage::MetadataType type)
{
    m_dataManager->flushMetadata(type);
}

nx::sdk::ErrorCode Engine::saveMetadata(
    const char* /*deviceId*/,
    nx::sdk::cloud_storage::MetadataType type,
    const char* data)
{
    NX_OUTPUT << __func__  << ", type: " << toString(type) << ", data: " << data;
    try
    {
        std::lock_guard lock(m_mutex);
        if (!m_dataManager)
            throw std::logic_error("Plugin has not been properly initialized");

        nx::sdk::ErrorCode result = nx::sdk::ErrorCode::otherError;
        switch (type)
        {
            case sdk::cloud_storage::MetadataType::bookmark:
                result = m_dataManager->saveBookmark(nx::sdk::cloud_storage::Bookmark(data));
                break;
            case sdk::cloud_storage::MetadataType::motion:
                result = m_dataManager->saveMotion(nx::sdk::cloud_storage::Motion(data));
                break;
            case sdk::cloud_storage::MetadataType::analytics:
                result = m_dataManager->saveObjectTrack(nx::sdk::cloud_storage::ObjectTrack(data));
                break;
        }

        NX_OUTPUT
            << __func__ << ", type: " << toString(type)
            << ". Result: " << nx::sdk::toString(result);

        return result;
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ", type: " << toString(type) << ". Fail. Error: " << e.what();
        return nx::sdk::ErrorCode::internalError;
    }
}

bool Engine::isOnline() const
{
    return true;
}

nx::sdk::ErrorCode Engine::storageSpace(nx::sdk::cloud_storage::StorageSpace* storageSpace) const
{
    storageSpace->freeSpace = 1000 * 1024 * 1024 * 1024LL;
    storageSpace->totalSpace = 1000 * 1024 * 1024 * 1024LL;
    return nx::sdk::ErrorCode::noError;
}

void Engine::doFetchTrackImage(
    const char* objectTrackId,
    nx::sdk::cloud_storage::TrackImageType type,
    nx::sdk::Result<nx::sdk::IString*>* outResult) const
{
    try
    {
        std::lock_guard lock(m_mutex);
        const auto image64 = m_dataManager->fetchTrackImage(objectTrackId, type);
        *outResult = nx::sdk::Result<nx::sdk::IString*>(new nx::sdk::String(image64));
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << __func__ << ": Failed to fetch BestShot image: " << e.what();
        *outResult = nx::sdk::Result<nx::sdk::IString*>(
            nx::sdk::Error(nx::sdk::ErrorCode::noData, new nx::sdk::String(e.what())));
    }
}

} // nx::vms_server_plugins::cloud_storage::stub
