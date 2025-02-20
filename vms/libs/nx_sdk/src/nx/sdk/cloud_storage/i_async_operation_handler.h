// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_data_list.h"
#include "i_device_agent.h"
#include "i_engine.h"

namespace nx::sdk::cloud_storage {

/**
 * Storage bucket description. Each media file might be stored on a separate bucket. Media files
 * are reported with the bucket id index, this struct provides a way to resolve this index to
 * the actual bucket url. This url must be used when fetching media files back using the
 * IDeviceAgent::createStreamReader() function.
 */
class IBucketDescriptionList:
    public Interface<IBucketDescriptionList>,
    public IDataList
{
public:
    /**
     * Must be called before get() to obtain Url length. If there's no next element, this function
     * will return -1;
     */
    virtual int urlLen() const = 0;
    virtual bool get(char* url, int* bucketId) const = 0;
};

/**
 * This struct describes previously recorded media file.
 */
class IMediaChunkList:
    public Interface<IMediaChunkList>,
    public IDataList
{
public:
    virtual bool get(int64_t* outStartTimeMs, int64_t* outDurationMs, int* outBucketId) const = 0;
};

/**
 * Added and removed time period lists per one stream.
 */
class IIndexArchive: public Interface<IIndexArchive>
{
public:
    virtual const IMediaChunkList* addedChunks() const = 0;
    virtual const IMediaChunkList* removedChunks() const = 0;
    virtual int streamIndex() const = 0;
};

/**
 * A Device Archive consisting of the list of the stream Archives.
 */
class IDeviceArchive: public Interface<IDeviceArchive>
{
public:
    virtual const IList<IIndexArchive>* indexArchive() = 0;
    virtual IDeviceAgent* deviceAgent() const = 0;
};

/**
 * A pointer to the this class instance is provided to the engine when
 * IIntegration::createEngine() is called. The engine should use it to periodically inform Server
 * about any Archive changes and report results of the recent save operations.
 */
class IAsyncOperationHandler: public Interface<IAsyncOperationHandler>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IAsyncOperationHandler"); }

    /**
     * Engine should call this periodically to check if something is changed on the backend.
     * I.e. some data was removed due to the retention policy, new data has been recorded,
     * new device appeared.
     * The deviceArchive may be destroyed by the plugin right after the onArchiveUpdated function
     * has returned.
     */
    virtual void onArchiveUpdated(
        const char* engineId,
        const char* lastSequenceId,
        nx::sdk::ErrorCode errorCode,
        const IList<IDeviceArchive>* deviceArchive) const = 0;

    virtual void onBucketDescriptionUpdated(
        const char* engineId,
        const IBucketDescriptionList* bucketDescriptionList) const = 0;

    virtual void onSaveOperationCompleted(
        const char* engineId,
        MetadataType metadataType,
        nx::sdk::ErrorCode errorCode) const = 0;
};

} // namespace nx::sdk::cloud_storage
