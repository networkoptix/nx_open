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

/**
 * This struct describes previously recorded media file.
 */
class MediaChunk: public IMediaChunk
{
public:
    MediaChunk(int64_t startTimeMs, int64_t durationMs, const char* bucketUrl);

    virtual int64_t startTimeMs() const override;
    virtual int64_t durationMs() const override;
    virtual const char* locationUrl() const override;

    std::string toString() const;
    bool operator<(const MediaChunk& other) const;

private:
    int64_t m_startTimeMs = -1;
    int64_t m_durationMs = -1;
    const char* m_bucketUrl = nullptr;
};

std::string toString(const std::vector<MediaChunk>& chunks);

using IndexData = std::map<ChunkOperation, std::vector<MediaChunk>>;
using DeviceData = std::map<int /*streamIndex*/, IndexData>;
using CloudChunkData = std::map<std::string, DeviceData>;

class MediaChunkList:
    public nx::sdk::RefCountable<IMediaChunkList>,
    public DataList<MediaChunk>
{
public:
    MediaChunkList(const std::vector<MediaChunk>& chunks);

    virtual void goToBeginning() const override;
    virtual void next() const override;
    virtual bool atEnd() const override;
    virtual const IMediaChunk* get() const override;

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
