// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <optional>

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/device_archive.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/to_string.h>

#include "device_agent.h"
#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

using namespace std::chrono;

std::optional<int64_t> getTimePointFromSeqId(const std::string& s)
{
    if (s.empty())
        return std::nullopt;

    try
    {
        return std::stol(s);
    }
    catch(...)
    {
        return std::nullopt;
    }
}

static constexpr int64_t kReportPeriodMs = 5*1000;

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

void Engine::startAsyncTasks(const char* lastSequenceId)
{
    m_worker = std::thread(
        [this, lastSequenceId = std::string(lastSequenceId)]()
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

                const auto nowMs =
                    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                if (m_lastReportTimePoint
                    && *m_lastReportTimePoint + kReportPeriodMs > nowMs)
                {
                    lock.unlock();
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                using namespace nx::sdk::cloud_storage;
                constexpr static auto kSafetyScanPeriod = 2 * 60 * 1000;
                const auto lastPointFromArgument = getTimePointFromSeqId(lastSequenceId);
                if (lastPointFromArgument)
                    m_lastScanTimePoint = lastPointFromArgument;

                std::optional<int64_t> scanStartTime = m_lastScanTimePoint
                    ? std::max<int64_t>(0, *m_lastScanTimePoint - kSafetyScanPeriod)
                    : std::optional<int64_t>(std::nullopt);

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

                    for (const auto& entry: deviceArchiveIndex.chunksPerStream)
                    {
                        NX_OUTPUT << "New chunks for device '"
                            << *deviceId << "': " << toString(entry.second);

                        if (!entry.second.empty())
                        {
                            const auto lastChunkStartTime = entry.second.back().startTimeMs();
                            if (lastChunkStartTime > scanStartTime)
                                scanStartTime = lastChunkStartTime;
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
                    result.addItem(nx::sdk::makePtr<nx::sdk::cloud_storage::DeviceArchive>(
                        deviceAgent, deviceIdToDeviceData.second));
                }

                // Real-life implementation should manage its free space by itself and
                // report data removal along with the newly added time periods here.
                const auto currentSequenceId =
                    scanStartTime ? std::to_string(*scanStartTime) : "0";
                m_handler->onArchiveUpdated(
                    m_integrationId.data(), currentSequenceId.data(), nx::sdk::ErrorCode::noError, &result);

                m_lastScanTimePoint = scanStartTime;
                m_lastReportTimePoint =
                    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
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

} // nx::vms_server_plugins::cloud_storage::stub
