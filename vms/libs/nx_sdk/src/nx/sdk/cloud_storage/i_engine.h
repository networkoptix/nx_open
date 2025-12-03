// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/i_integration.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/result.h>

#include "i_device_agent.h"

namespace nx::sdk::cloud_storage {

enum class MetadataType
{
    motion
};

struct StorageSpace
{
    int64_t totalSpace = -1;
    int64_t freeSpace = -1;
};

enum class TrackImageType
{
    bestShot,
    title,
};

/**
 * Engine is an abstraction of a backend communication entity. It's something that knows how to
 * interact with the particular backend endpoint. Currently, Server won't ask plugin for
 * multiple instances of the Engine. But in future it may be implemented. So that each plugin
 * can handle multiple cloud storages each defined by its url.
 * The instance of this class is used, on one hand, by the Server to add new devices and subscribe
 * to archive change notifications. On the other hand, the plugin may track and process all
 * external changes to the archive and/or devices here and report them to the Server.
 */
class IEngine: public Interface<IEngine>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IEngine"); }

    protected: virtual void doObtainDeviceAgent(
        Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo) = 0;

    /** Add a new device agent described by deviceInfo. If the device agent already has been
     * created, it should be returned without reinstantiation.
     */
    public: virtual Result<IDeviceAgent*> obtainDeviceAgent(const IDeviceInfo* deviceInfo)
    {
        Result<IDeviceAgent*> result;
        doObtainDeviceAgent(&result, deviceInfo);
        return result;
    }

    protected: virtual void doQueryMotionTimePeriods(
        const char* filter, Result<IString*>* outResult) = 0;

    /**
     * Query motion time periods accordingly to the given filter.
     * `filter` is the JSON representation of the `MotionFilter`
     * `result` should be the JSON representation of the `TimePeriodList`.
     */
    public: Result<IString*> queryMotionTimePeriods(const char* filter)
    {
        Result<IString*> result;
        doQueryMotionTimePeriods(filter, &result);
        return result;
    }

    protected:

    /**
     * @param data A null-terminated string containing the JSON of the corresponding metadata object.
     * Refer to `ObjectTrack` and `Motion` objects in the
     * sdk/cloud_storage/helpers/data.h for details.
     * @note If `saveMetadata` returned `inProgress` VMS Server won't call it again until the
     * onSaveOperationCompleted is called by the plugin.
     */
    public: virtual ErrorCode saveMetadata(
        const char* deviceId, MetadataType type, const char* data) = 0;

    /**
     * Plugin must send all pending buffered data of the given type immediately when this function
     * is called. If there's nothing to send `onSaveOperationCompleted` should be called with the
     * `noError` result.
     */
    public: virtual void flushMetadata(MetadataType type) = 0;

    /**
     * No async handlers should be called by the plugin before this function has been called.
     */
    public: virtual void startAsyncTasks(const char* lastSequenceId) = 0;

    /**
     * No async handlers should be called by the plugin after this function has been called.
     */
    public: virtual void stopAsyncTasks() = 0;

    /**
     * Check if plugin backend is operational. This function will be periodically called by
     * Server.
     */
    public: virtual bool isOnline() const = 0;

    /**
     * Get backend storage space information.
     */
    public: virtual ErrorCode storageSpace(StorageSpace* storageSpace) const = 0;
};

} // namespace nx::sdk::cloud_storage
