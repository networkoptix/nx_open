// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_archive.h"

namespace nx::sdk::cloud_storage {

using namespace std::chrono;

int64_t timePointToInt64Ms(system_clock::time_point tp)
{
    return duration_cast<milliseconds>(tp.time_since_epoch()).count();
}

std::string MediaChunk::toString() const
{
    return "(" + std::to_string(timePointToInt64Ms(startPoint)) +
        ", " + std::to_string(durationMs.count()) +
        ", " + std::to_string(bucketId) + ")";
}

bool MediaChunk::operator<(const MediaChunk& other) const
{
    if (bucketId != other.bucketId)
        return bucketId < other.bucketId;

    if (startPoint != other.startPoint)
        return startPoint < other.startPoint;

    if (durationMs != other.durationMs)
        return durationMs < other.durationMs;

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

BucketDescriptionList::BucketDescriptionList(std::vector<BucketDescription> data):
    DataList<BucketDescription>(std::move(data))
{}

void BucketDescriptionList::goToBeginning() const
{
    Base::goToBeginning();
}

void BucketDescriptionList::next() const
{
    Base::next();
}

bool BucketDescriptionList::atEnd() const
{
    return Base::atEnd();
}

int BucketDescriptionList::urlLen() const
{
    const auto* b = Base::get();
    if (!b)
        return -1;

    return b->url.size() + 1;
}

bool BucketDescriptionList::get(char* url, int* bucketId) const
{
    const auto* b = Base::get();
    if (!b)
        return false;

    *bucketId = b->bucketId;
    ::strncpy(url, b->url.data(), b->url.size());
    url[b->url.size()] = '\0';
    return true;
}

MediaChunkList::MediaChunkList(std::vector<MediaChunk> chunks):
    DataList<MediaChunk>(std::move(chunks))
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

bool MediaChunkList::get(
    int64_t* outStartTimeMs, int64_t* outDurationMs, int* outBucketId) const
{
    const auto* chunk = Base::get();
    if (!chunk)
        return false;

    *outStartTimeMs = timePointToInt64Ms(chunk->startPoint);
    *outDurationMs = chunk->durationMs.count();
    *outBucketId = chunk->bucketId;
    return true;
}

IndexArchive::IndexArchive(int streamIndex, const IndexData& indexData):
    m_streamIndex(streamIndex)
{
    m_addedChunks.reset(new MediaChunkList({}));
    m_removedChunks.reset(new MediaChunkList({}));
    for (const auto& operationToPeriod: indexData)
    {
        switch (operationToPeriod.first)
        {
            case ChunkOperation::add:
                m_addedChunks.reset(new MediaChunkList(std::move(operationToPeriod.second)));
                break;
            case ChunkOperation::remove:
                m_removedChunks.reset(new MediaChunkList(std::move(operationToPeriod.second)));
                break;
        }
    }
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
