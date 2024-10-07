// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/result.h>

#include "i_device_agent.h"

namespace nx::sdk::cloud_storage {

enum class MetadataType
{
    motion,
    analytics,
    bookmark,
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

    protected: virtual void doQueryBookmarks(const char* filter, Result<IString*>* outResult) = 0;

    /**
     * Query bookmarks accordingly to the given filter.
     * `filter` is the JSON representation of the `BookmarkFilter`.
     * The result should contain a JSON representation of a list of the `Bookmark`.
     */
    public: Result<IString*> queryBookmarks(const char* filter)
    {
        Result<IString*> result;
        doQueryBookmarks(filter, &result);
        return result;
    }

    public: virtual ErrorCode deleteBookmark(const char* bookmarkId) = 0;

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

    protected: virtual void doQueryAnalytics(const char* filter, Result<IString*>* outResult) = 0;

    /**
     * Query object tracks accordingly to the given filter.
     * `filter` is the JSON representation of the `AnalyticsFilter`.
     * `result` should be the JSON representation of the `AnalyticsLookupResult`.
     */
    public: Result<IString*> queryAnalytics(const char* filter)
    {
        Result<IString*> result;
        doQueryAnalytics(filter, &result);
        return result;
    }

    protected: virtual void doQueryAnalyticsTimePeriods(
        const char* filter, Result<IString*>* outResult) = 0;

    /**
     * Query analytics time periods (extracted from object tracks) accordingly to the given filter.
     * `filter` is the JSON representation of the `AnalyticsFilter`.
     * `result` should be the JSON representation of the `TimePeriodList`.
     */
    public: Result<IString*> queryAnalyticsTimePeriods(const char* filter)
    {
        Result<IString*> result;
        doQueryAnalyticsTimePeriods(filter, &result);
        return result;
    }

    /**
     * @param data A null-terminated string containing the JSON of the corresponding metadata object.
     * Refer to `Bookmark`, `ObjectTrack` and `Motion` objects in the
     * sdk/cloud_storage/helpers/data.h for details.
     */
    public: virtual ErrorCode saveMetadata(
        const char* deviceId, int64_t timestampUs, MetadataType type, const char* data) = 0;

    /**
     * Add a  image to the `ObjectTrack`. `data` is the JSON representation of the
     * nx::sdk::cloud_storage::Image struct.
     */
    public: virtual ErrorCode saveTrackImage(const char* data, TrackImageType type) = 0;

    /**
     * Fetch a best shot image associated with the given objectTrackId. Result string should
     * contain the JSON representation of the nx::sdk::cloud_storage::Image struct.
     */
    public: Result<IString*> fetchTrackImage(const char* objectTrackId, TrackImageType type) const
    {
        Result<IString*> result;
        doFetchTrackImage(objectTrackId, type, &result);
        return result;
    }

    protected: virtual void doFetchTrackImage(
        const char* objectTrackId, TrackImageType type, Result<IString*>* outResult) const = 0;

    /**
     * No IArchiveUpdateHandler::onArchiveUpdated() should be called by the plugin before this
     * function has been called.
     */
    public: virtual void startNotifications() = 0;

    /**
     * No IArchiveUpdateHandler::onArchiveUpdated() should be called by the plugin after this
     * function has been called.
     */
    public: virtual void stopNotifications() = 0;

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
