// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>

#include <nx/sdk/cloud_storage/helpers/time_periods.h>
#include <nx/sdk/cloud_storage/i_archive_update_handler.h>
#include <nx/sdk/helpers/list.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::cloud_storage {

enum class ChunkOperation
{
    add,
    remove,
};

using IndexData = std::map<
    ChunkOperation,
    std::vector<nx::sdk::cloud_storage::TimePeriod>>;

using DeviceData = std::map<
    int /*streamIndex*/,
    IndexData>;

using CloudChunkData = std::map<
    std::string /*deviceId*/,
    DeviceData>;

class IndexArchive: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IIndexArchive>
{
public:
    IndexArchive(int streamIndex, const IndexData& indexData);

    virtual const nx::sdk::cloud_storage::ITimePeriods* addedTimePeriods() const override;
    virtual const nx::sdk::cloud_storage::ITimePeriods* removedTimePeriods() const override;
    virtual int streamIndex() const override;

private:
    const int m_streamIndex;
    nx::sdk::cloud_storage::TimePeriods m_addedPeriods;
    nx::sdk::cloud_storage::TimePeriods m_removedPeriods;
};

class DeviceArchive: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IDeviceArchive>
{
public:
    DeviceArchive(
        nx::sdk::cloud_storage::IDeviceAgent* deviceAgent,
        const DeviceData& deviceData);

    virtual nx::sdk::IList<nx::sdk::cloud_storage::IIndexArchive>* indexArchive() override;
    virtual nx::sdk::cloud_storage::IDeviceAgent* deviceAgent() const override;

private:
    nx::sdk::cloud_storage::IDeviceAgent* m_deviceAgent;
    const DeviceData& m_data;
    nx::sdk::List<nx::sdk::cloud_storage::IIndexArchive> m_indexArchiveList;
};

} // namespace nx::sdk::cloud_storage
