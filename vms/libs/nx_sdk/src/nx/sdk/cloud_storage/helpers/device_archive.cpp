// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_archive.h"

namespace nx::sdk::cloud_storage {

IndexArchive::IndexArchive(int streamIndex, const IndexData& indexData):
    m_streamIndex(streamIndex)
{
    std::vector<nx::sdk::cloud_storage::TimePeriod> added;
    std::vector<nx::sdk::cloud_storage::TimePeriod> removed;
    for (const auto& operationToPeriod: indexData)
    {
        switch (operationToPeriod.first)
        {
            case ChunkOperation::add:
                added = operationToPeriod.second;
                break;
            case ChunkOperation::remove:
                removed = operationToPeriod.second;
                break;
        }
    }

    m_addedPeriods.setPeriods(added);
    m_removedPeriods.setPeriods(removed);
}

const nx::sdk::cloud_storage::ITimePeriods* IndexArchive::addedTimePeriods() const
{
    return &m_addedPeriods;
}

const nx::sdk::cloud_storage::ITimePeriods* IndexArchive::removedTimePeriods() const
{
    return &m_removedPeriods;
}

int IndexArchive::streamIndex() const
{
    return m_streamIndex;
}

DeviceArchive::DeviceArchive(
    nx::sdk::cloud_storage::IDeviceAgent* deviceAgent,
    const DeviceData& deviceData)
    :
    m_deviceAgent(deviceAgent),
    m_data(deviceData)
{
    for (const auto& indexToIndexData: m_data)
    {
        m_indexArchiveList.addItem(
            new IndexArchive(indexToIndexData.first, indexToIndexData.second));
    }
}

nx::sdk::IList<nx::sdk::cloud_storage::IIndexArchive>* DeviceArchive::indexArchive()
{
    return &m_indexArchiveList;
}

nx::sdk::cloud_storage::IDeviceAgent* DeviceArchive::deviceAgent() const
{
    return m_deviceAgent;
}

} // namespace nx::sdk::cloud_storage
