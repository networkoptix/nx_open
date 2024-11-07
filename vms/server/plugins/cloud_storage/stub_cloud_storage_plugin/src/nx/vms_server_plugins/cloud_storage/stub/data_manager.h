// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <camera/camera_plugin.h>
#include <nx/kit/json.h>
#include <nx/sdk/cloud_storage/helpers/data.h>
#include <nx/sdk/cloud_storage/i_device_agent.h>
#include <nx/sdk/cloud_storage/i_media_data_packet.h>
#include <nx/sdk/i_device_info.h>

namespace nx::vms_server_plugins::cloud_storage::stub {

class FileImpl;

class File
{
public:
    File(const std::string& path);
    ~File();

    template<typename Container>
    Container readAll() const;

    template<typename Container>
    void writeAll(const Container& data, bool replaceContents);

private:
    std::unique_ptr<FileImpl> m_fileImpl;
};

struct MediaFileHeader;

class WritableMediaFile
{
public:
    WritableMediaFile(
        const std::shared_ptr<std::mutex>& mutex,
        const std::string& path,
        std::chrono::milliseconds startTime,
        const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
        const char* opaqueMetadata);

    ~WritableMediaFile();

    void writeMediaPacket(const nx::sdk::cloud_storage::IMediaDataPacket* packet);
    void close(std::chrono::milliseconds duration);
    int size() const;

private:
    std::shared_ptr<std::mutex> m_mutex;
    std::unique_ptr<File> m_file;
    const std::string m_path;
    std::chrono::milliseconds m_startTime{};
    std::vector<uint8_t> m_buffer;
};

class ReadableMediaFile
{
public:
    ReadableMediaFile(const std::string& path);
    ~ReadableMediaFile();

    std::vector<nx::sdk::cloud_storage::CodecInfoData> codecInfo() const;
    std::string opaqueMetadata() const;
    std::optional<nx::sdk::cloud_storage::MediaPacketData> getNextPacket() const;
    std::string name() const;
    void seek(
        int64_t timestamp,
        bool findKeyFrame,
        int64_t* selectedPosition);

    int size() const;

private:
    std::vector<uint8_t> m_fileData;
    std::unique_ptr<MediaFileHeader> m_header;
    int m_begin = 0;
    mutable int m_current = 0;
    std::string m_name;
};

struct DeviceArchiveIndex
{
    std::map<int, std::vector<nx::sdk::cloud_storage::TimePeriod>> timePeriodsPerStream;
    nx::sdk::cloud_storage::DeviceDescription deviceDescription;
};

struct ArchiveIndex
{
    std::vector<DeviceArchiveIndex> deviceArchiveIndexList;

    void sort();
};

class DataManager
{
public:
    using SaveHandler =
        std::function<void(nx::sdk::cloud_storage::MetadataType, nx::sdk::ErrorCode)>;

    DataManager(const std::string& workDir);
    ~DataManager();

    void setSaveHandler(SaveHandler handler);
    nx::sdk::ErrorCode saveBookmark(const nx::sdk::cloud_storage::Bookmark& data);
    void deleteBookmark(const char* id);
    std::string queryBookmarks(const nx::sdk::cloud_storage::BookmarkFilter& filter) const;

    nx::sdk::ErrorCode saveMotion(const nx::sdk::cloud_storage::Motion& data);
    std::string queryMotion(const nx::sdk::cloud_storage::MotionFilter& filter) const;

    nx::sdk::ErrorCode saveObjectTrack(const nx::sdk::cloud_storage::ObjectTrack& data);
    std::string queryAnalytics(const nx::sdk::cloud_storage::AnalyticsFilter& filter) const;
    std::string queryAnalyticsPeriods(const nx::sdk::cloud_storage::AnalyticsFilter& filter) const;

    void addDevice(const nx::sdk::cloud_storage::DeviceDescription& deviceDescription);
    std::unique_ptr<WritableMediaFile> writableMediaFile(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::milliseconds timestamp,
        const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
        const char* opaqueMetadata) const;

    std::unique_ptr<ReadableMediaFile> readableMediaFile(
        const std::string& deviceId,
        int streamIndex,
        int64_t startTimeMs,
        int64_t durationMs) const;

    ArchiveIndex getArchive(std::optional<std::chrono::system_clock::time_point> startTime) const;
    void saveTrackImage(
        const char* data,
        nx::sdk::cloud_storage::TrackImageType type);
    std::string fetchTrackImage(const char* objectTrackId, nx::sdk::cloud_storage::TrackImageType type);

    void run();
    void flushMetadata(nx::sdk::cloud_storage::MetadataType type);

private:
    std::string m_workDir;
    std::shared_ptr<std::mutex> m_mutex{std::make_shared<std::mutex>()};
    std::vector<nx::sdk::cloud_storage::Bookmark> m_cloudBookmarks;
    std::vector<nx::sdk::cloud_storage::Motion> m_cloudMotion;
    std::vector<nx::sdk::cloud_storage::ObjectTrack> m_cloudAnalytics;
    std::queue<nx::sdk::cloud_storage::Bookmark> m_bookmarksToSend;
    std::queue<nx::sdk::cloud_storage::Motion> m_motionToSend;
    std::queue<nx::sdk::cloud_storage::ObjectTrack> m_analyticsToSend;
    SaveHandler m_saveHandler;
    std::thread m_workThread;
    std::atomic<bool> m_needToStop{false};
    std::condition_variable m_cond;
    bool m_needToFlushBookmarks{false};
    bool m_needToFlushMotion{false};
    bool m_needToFlushAnalytics{false};

private:
    std::string devicePath(const std::string& deviceId) const;

    template<typename Data>
    std::queue<Data>* dataQueue();

    template<typename Data>
    std::vector<Data>* cloudData();

    template<typename Data>
    bool* needToFlush();

    template<typename Data>
    nx::sdk::cloud_storage::MetadataType metadataType() const;

    template<typename Data>
    void processQueue(std::queue<Data>& dataQueue, std::unique_lock<std::mutex>& lock);

    template<typename Data>
    nx::sdk::ErrorCode saveMetadataImpl(const Data& data);

    template<typename Data>
    void saveObject(const Data& data, const std::string& fileName);

    template<typename Data>
    std::vector<Data> allObjects(const std::string& fileName) const;

    template<typename Data>
    std::vector<Data> allObjectsNoLock(const std::string& fileName) const;

    template<typename Data>
    std::optional<Data> oneObject(const std::string& fileName) const;

    template<typename DataList>
    void flushToFile(const DataList& data, const std::string& fileName);

    template<typename Data>
    std::vector<Data> load(const std::string& fileName) const;

    std::vector<nx::kit::Json> loadListNoLock(const std::string& fileName) const;

    std::string mediaFilePath(
        const std::string& deviceId, int streamIndex, std::chrono::milliseconds timestamp) const;

    std::string mediaFilePath(
        const std::string& deviceId, int streamIndex, int64_t startTimeMs, int64_t durationMs) const;
};

} //namespace nx::vms_server_plugins::cloud_storage::stub
