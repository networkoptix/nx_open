// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_archive.h"

namespace nx::sdk::cloud_storage {

using namespace std::chrono;

int64_t timePointToInt64Ms(system_clock::time_point tp)
{
    return duration_cast<milliseconds>(tp.time_since_epoch()).count();
}

MediaChunk::MediaChunk(int64_t startTimeMs, int64_t durationMs, const char* bucketUrl):
    m_startTimeMs(startTimeMs),
    m_durationMs(durationMs),
    m_bucketUrl(bucketUrl)
{}

int64_t MediaChunk::startTimeMs() const
{
    return m_startTimeMs;
}

int64_t MediaChunk::durationMs() const
{
    return m_durationMs;
}

const char* MediaChunk::locationUrl() const
{
    return m_bucketUrl;
}

std::string MediaChunk::toString() const
{
    return "(" + std::to_string(m_startTimeMs) +
        ", " + std::to_string(m_durationMs) +
        ", " + std::string(m_bucketUrl) + ")";
}

bool MediaChunk::operator<(const MediaChunk& other) const
{
    int urlComparisonRes = strcmp(m_bucketUrl, other.m_bucketUrl);
    if (urlComparisonRes != 0)
        return urlComparisonRes < 0;

    if (m_startTimeMs != other.m_startTimeMs)
        return m_startTimeMs < other.m_startTimeMs;

    if (m_durationMs != other.m_durationMs)
        return m_durationMs < other.m_durationMs;

    return false;
}

std::string toString(const std::vector<MediaChunk>& chunks)
{
    std::string result = "[";
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        result += chunks[i].toString();
        if (i != chunks.size() - 1)
            result += " ";
    }

    result += "]";
    return result;
}

MediaChunkList::MediaChunkList(const std::vector<MediaChunk>& chunks):
    DataList<MediaChunk>(chunks)
{
}

void MediaChunkList::goToBeginning() const
{
    Base::goToBeginning();
}

void MediaChunkList::next() const
{
    Base::next();
}

bool MediaChunkList::atEnd() const
{
    return Base::atEnd();
}

const IMediaChunk* MediaChunkList::get() const
{
    return Base::get();
}

IndexArchive::IndexArchive(int streamIndex, const IndexData& indexData)
    :
    m_streamIndex(streamIndex)
{
    for (const auto& operationToPeriod: indexData)
    {
        switch (operationToPeriod.first)
        {
            case ChunkOperation::add:
                m_addedChunks.reset(
                    new MediaChunkList(operationToPeriod.second));
                break;
            case ChunkOperation::remove:
                m_removedChunks.reset(
                    new MediaChunkList(operationToPeriod.second));
                break;
        }
    }

    if (!m_addedChunks)
        m_addedChunks.reset(new MediaChunkList({}));

    if (!m_removedChunks)
        m_removedChunks.reset(new MediaChunkList({}));
}

const IMediaChunkList* IndexArchive::addedChunks() const
{
    return m_addedChunks.get();
}

const IMediaChunkList* IndexArchive::removedChunks() const
{
    return m_removedChunks.get();
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
        m_indexArchiveList.addItem(makePtr<IndexArchive>(
            indexToIndexData.first, indexToIndexData.second));
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
