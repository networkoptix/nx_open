// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>
#include <chrono>

#include <nx/sdk/cloud_storage/helpers/data_list.h>
#include <nx/sdk/cloud_storage/i_async_operation_handler.h>
#include <nx/sdk/helpers/list.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::cloud_storage {

enum class ChunkOperation
{
    add,
    remove,
};

struct MediaChunk
{
    std::chrono::system_clock::time_point startPoint{};
    std::chrono::milliseconds durationMs{-1};
    int bucketId = -1;

    bool operator<(const MediaChunk& other) const;

    std::string toString() const;
};

std::string toString(const std::vector<MediaChunk>& chunks);

struct BucketDescription
{
    std::string url;
    int bucketId = -1;
};

using IndexData = std::map<ChunkOperation, std::vector<MediaChunk>>;
using DeviceData = std::map<int /*streamIndex*/, IndexData>;
using CloudChunkData = std::map<std::string, DeviceData>;

class BucketDescriptionList:
    public nx::sdk::RefCountable<IBucketDescriptionList>,
    public DataList<BucketDescription>
{
public:
    BucketDescriptionList(std::vector<BucketDescription> data);

    virtual void goToBeginning() const override;
    virtual void next() const override;
    virtual bool atEnd() const override;
    virtual int urlLen() const override;
    virtual bool get(char* url, int* bucketId) const override;

private:
    using Base = DataList<BucketDescription>;
};

/**
 * This struct describes previously recorded media file.
 */
class MediaChunkList:
    public nx::sdk::RefCountable<IMediaChunkList>,
    public DataList<MediaChunk>
{
public:
    MediaChunkList(std::vector<MediaChunk> chunks);

    virtual void goToBeginning() const override;
    virtual void next() const override;
    virtual bool atEnd() const override;
    virtual bool get(int64_t* outStartTimeMs, int64_t* durationMs, int* bucketId) const override;

private:
    using Base = DataList<MediaChunk>;
};

class IndexArchive: public nx::sdk::RefCountable<IIndexArchive>
{
public:
    IndexArchive(int streamIndex, const IndexData& indexData);

    virtual const IMediaChunkList* addedChunks() const override;
    virtual const IMediaChunkList* removedChunks() const override;
    virtual int streamIndex() const override;

private:
    const int m_streamIndex;
    Ptr<IMediaChunkList> m_addedChunks;
    Ptr<IMediaChunkList> m_removedChunks;
};

class DeviceArchive: public nx::sdk::RefCountable<IDeviceArchive>
{
public:
    DeviceArchive(
        IDeviceAgent* deviceAgent,
        const DeviceData& deviceData);

    virtual IList<IIndexArchive>* indexArchive() override;
    virtual IDeviceAgent* deviceAgent() const override;

private:
    IDeviceAgent* m_deviceAgent;
    const DeviceData& m_data;
    List<IIndexArchive> m_indexArchiveList;
};

} // namespace nx::sdk::cloud_storage
