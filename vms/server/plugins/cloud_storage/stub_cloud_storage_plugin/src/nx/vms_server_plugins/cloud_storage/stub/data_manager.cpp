// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "data_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <map>

#include "nx/sdk/cloud_storage/helpers/data.h"

#if defined (_WIN32)
    #include <Shlwapi.h>
    #include <windows.h>

    #pragma comment(lib, "Shlwapi.lib")

#elif defined (__unix__) || defined(__APPLE__)
    #include <dirent.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <string.h>
    #include <unistd.h>

    #include <sys/stat.h>

#endif // _WIN32

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/algorithm.h>

#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

const char* kBookmarkFileName = "bookmarks.txt";
const char* kMotionFileName = "motion.txt";
const char* kAnalyticsFileName = "analytics.txt";
const char* kDeviceDescriptionFileName = "device_description.txt";
const char* kMediaFileExtenstion = ".dat";
const char* kImageFolderName = "images";
const char* kImageExtension = ".img";

using namespace std::chrono;

constexpr size_t kMaxSendBufferSize = 5;

struct FileInfo
{
    FileInfo() = default;
    FileInfo(const std::string& path);

    std::string fullPath;
    std::string extension;
    std::string baseName;
    std::string parentDir;
    bool isDir = false;
};

using FileInfoList = std::vector<FileInfo>;

template<typename Container>
void append(const void* data, size_t size, Container* out)
{
    out->resize(out->size() + size);
    memcpy(out->data() + out->size() - size, data, size);
}

template<typename T, typename Container>
T read(const Container& c, int* outPos)
{
    T result;
    if (*outPos + sizeof(result) > c.size())
        throw std::runtime_error("Not enough data");

    memcpy(&result, c.data() + *outPos, sizeof(result));
    *outPos += sizeof(result);
    return result;
}

template<typename T, typename Container>
T readBytes(const Container& c, size_t size, int* outPos, int padding = 0)
{
    T result;
    if (*outPos + size > c.size())
        throw std::runtime_error("Not enough data");

    result.resize(size + padding);
    memcpy(result.data(), c.data() + *outPos, size);
    *outPos += size;
    return result;
}

nx::sdk::cloud_storage::MediaPacketData mediaPacketData(
    const std::vector<uint8_t>& data, int* outPos)
{
    nx::sdk::cloud_storage::MediaPacketData result;
    result.dataSize = read<size_t>(data, outPos);
    result.data = readBytes<std::vector<uint8_t>>(
        data, result.dataSize, outPos, nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE);

    result.compressionType = read<nxcip::CompressionType>(data, outPos);
    result.timestampUs = read<nxcip::UsecUTCTimestamp>(data, outPos);
    result.type = read<nx::sdk::cloud_storage::IMediaDataPacket::Type>(data, outPos);
    result.channelNumber = read<int>(data, outPos);
    result.isKeyFrame = read<bool>(data, outPos);
    size_t size = read<size_t>(data, outPos);
    result.encryptionData = readBytes<std::vector<uint8_t>>(data, size, outPos);

    return result;
}

std::string joinPath(const std::string& p1, const std::string& p2)
{
    return p1 + "/" + p2;
}

std::string pathToImage(
    const std::string& workDir,
    const std::string& imageName,
    nx::sdk::cloud_storage::TrackImageType type)
{
    return sdk::cloud_storage::trackImageTypeToString(type)
        + "_" + joinPath(joinPath(workDir, kImageFolderName), imageName) + kImageExtension;
}

std::vector<uint8_t> mediaPacketDataToByteArray(
    const nx::sdk::cloud_storage::MediaPacketData& data)
{
    std::vector<uint8_t> result;
    size_t totalDataSize = data.data.size() + sizeof(data.compressionType)
        + sizeof(data.timestampUs) + sizeof(data.type) + sizeof(data.channelNumber)
        + sizeof(data.isKeyFrame) + data.encryptionData.size() + 2 * sizeof(size_t);

    result.reserve(totalDataSize * 1.2);
    size_t size = data.dataSize;
    append(&size, sizeof(size), &result);
    append(data.data.data(), size, &result);
    append(&data.compressionType, sizeof(data.compressionType), &result);
    append(&data.timestampUs, sizeof(data.timestampUs), &result);
    append(&data.type, sizeof(data.type), &result);
    append(&data.channelNumber, sizeof(data.channelNumber), &result);
    append(&data.isKeyFrame, sizeof(bool), &result);
    size = data.encryptionData.size();
    append(&size, sizeof(size), &result);
    append(data.encryptionData.data(), size, &result);

    return result;
}

std::vector<std::string> split(const std::string& s, char separator)
{
    std::vector<std::string> result;
    size_t pos = 0;
    while (true)
    {
        size_t next = s.find(separator, pos);
        if (next == std::string::npos)
            next = s.size();

        const auto sub = s.substr(pos, next - pos);
        if (!sub.empty())
            result.push_back(sub);

        if (next == s.size())
            break;

        pos = next + 1;
    }

    return result;
}

#if defined (_WIN32)

std::string winApiErrorString(DWORD code)
{
    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR) &buffer,
        0,
        NULL);

    std::string result(buffer, size);
    LocalFree(buffer);

    return result;
}

#else

// TODO: Consider moving to nx_kit and use it in other places instead of duplicating the code.
std::string safeStrerror(int errorCode)
{
    char errorBuffer[1024]; //< 1024 is taken from glibc perror implementation.
    #if defined(__APPLE__) || defined(ANDROID) || defined(__ANDROID__)
        // POSIX strerror_r implementation returns error code, not the error message. If the
        // function fails, return an empty string.
        if (!strerror_r(errorCode, errorBuffer, sizeof(errorBuffer)))
            std::string();
        return errorBuffer;
    #else
        return strerror_r(errorCode, errorBuffer, sizeof(errorBuffer));
    #endif
}

#endif // WIN32

void ensureDir(std::string path)
{
    if (path[path.size() - 1] == '/')
        path = path.substr(0, path.size() - 1);

    size_t separatorOffset = 0;
    while (true)
    {
        size_t separatorPos = path.find("/", separatorOffset);
        std::string subPath;
        if (separatorPos == std::string::npos)
            subPath = path;
        else
            subPath = path.substr(0, separatorPos);

        if (separatorPos != std::string::npos)
            separatorOffset = separatorPos + 1;

        if (subPath.empty())
        {
            if (separatorPos == std::string::npos)
            {
                throw std::runtime_error(
                    "Path to create is empty. Path: '" + path + "' subPath: '" + subPath + "'");
            }

            continue;
        }

        #if defined (_WIN32)
            if (PathFileExistsA(subPath.data()))
            {
                if (separatorPos == std::string::npos)
                    break;

                continue;
            }
        #elif defined(__unix__) || defined(__APPLE__)
            struct stat sd;
            if (::stat(subPath.data(), &sd) == 0 && S_ISDIR(sd.st_mode))
            {
                if (separatorPos == std::string::npos)
                    break;

                continue;
            }
        #endif // _WIN32

        #if defined(_WIN32)
            if (!CreateDirectoryA(subPath.data(), /*lpSecurityAttributes*/ nullptr))
                throw std::runtime_error(winApiErrorString(GetLastError()));
        #elif defined(__unix__) || defined(__APPLE__)
            if (mkdir(subPath.data(), 0777) != 0)
            {
                throw std::runtime_error(
                    std::string("mkdir failed for '") + subPath + "' " + safeStrerror(errno));
            }
        #endif // _WIN32

        if (separatorPos == std::string::npos)
            break;
    }
}

#if defined (_WIN32)

const char kPathSeparator = '/';

class FileImpl
{
public:
    FileImpl(const std::string& path): m_path(path)
    {
        openFile();
    }

    ~FileImpl()
    {
        CloseHandle(m_hFile);
    }

    template<typename Container>
    Container readAll() const
    {
        Container result;
        while (true)
        {
            uint8_t buf[4096];
            DWORD bytesRead = -1;
            const auto readResult = ReadFile(m_hFile, buf, sizeof(buf), &bytesRead, NULL);
            if (readResult == FALSE)
            {
                throw std::runtime_error(
                    "Read failed '" + m_path + "'. Error: " + winApiErrorString(GetLastError()));
            }

            append(buf, bytesRead, &result);
            if (bytesRead < sizeof(buf))
                break;
        }

        return result;
    }

    template<typename Container>
    void writeAll(const Container& data, bool replaceContents)
    {
        if (replaceContents)
        {
            if (!CloseHandle(m_hFile) || !DeleteFileA(m_path.data()))
            {
                throw std::runtime_error(
                    "Write all failed (close & delete) '" + m_path
                    + "'. Error: " + winApiErrorString(GetLastError()));
            }

            openFile();
        }

        DWORD writtenBytes = -1;
        const auto result = WriteFile(m_hFile, data.data(), data.size(), &writtenBytes, NULL);
        if (result == FALSE || writtenBytes != data.size())
        {
            throw std::runtime_error(
                "Write failed '" + m_path + "'. Error: " + winApiErrorString(GetLastError()));
        }
    }

private:
    const std::string m_path;
    HANDLE m_hFile;

    void openFile()
    {
        m_hFile = CreateFileA(
            m_path.data(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (m_hFile == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error(
                "Failed to open file '" + m_path + "'. Error: " + winApiErrorString(GetLastError()));
        }
    }
};

void moveFile(const std::string& oldPath, const std::string& newPath)
{
    const auto result = MoveFileA(oldPath.data(), newPath.data());
    if (result == FALSE)
    {
        throw std::runtime_error(
            "Move failed old: '" + oldPath + "', new: '" + newPath
            + "'. Error: " + winApiErrorString(GetLastError()));
    }
}

bool checkIfDir(const std::string& path)
{
    DWORD result = GetFileAttributesA(path.data());
    if (result == INVALID_FILE_ATTRIBUTES)
    {
        throw std::runtime_error(
            "Failed to retrieve file attributes. Path: '" + path + "'. Error: "
            + winApiErrorString(GetLastError()));
    }

    return result & FILE_ATTRIBUTE_DIRECTORY;
}

FileInfoList getFileInfoListImpl(
    const std::string& path, const std::optional<std::string>& ext = std::nullopt)
{
    FileInfoList result;
    WIN32_FIND_DATAA ffd;
    const auto searchPath = path + "/*";
    auto hFind = FindFirstFileA(searchPath.data(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return result;

    do
    {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        const auto fileInfo = FileInfo(joinPath(path, ffd.cFileName));
        if (!ext || fileInfo.extension == *ext)
            result.push_back(fileInfo);
    }
    while (FindNextFileA(hFind, &ffd));

    return result;
}

bool pathExists(const std::string& path)
{
    return PathFileExistsA(path.data());
}

#elif defined (__unix__)  || defined(__APPLE__)

const char kPathSeparator = '/';

bool checkIfDir(const std::string& path)
{
    struct stat sd;
    return ::stat(path.data(), &sd) == 0 && S_ISDIR(sd.st_mode);
}

bool pathExists(const std::string& path)
{
    struct stat statData;
    return stat(path.data(), &statData) == 0;
}

class FileImpl
{
public:
    FileImpl(const std::string& path): m_path(path)
    {
        openFile();
    }

    ~FileImpl()
    {
        ::close(m_fd);
    }

    template<typename Container>
    Container readAll() const
    {
        Container result;
        while (true)
        {
            uint8_t buf[4096];
            const auto readResult = ::read(m_fd, buf, sizeof(buf));
            if (readResult < 0)
            {
                throw std::runtime_error(
                    "Read failed '" + m_path + "'. Error: " + safeStrerror(errno));
            }

            append(buf, readResult, &result);
            if (readResult < (int) sizeof(buf))
                break;
        }

        return result;
    }

    template<typename Container>
    void writeAll(const Container& data, bool replaceContents)
    {
        if (replaceContents)
        {
            if (::close(m_fd) != 0 || ::unlink(m_path.data()) != 0)
            {
                throw std::runtime_error(
                    "Write all failed (close & delete) '" + m_path
                    + "'. Error: " + safeStrerror(errno));
            }

            openFile();
        }

        const auto writeResult = ::write(m_fd, data.data(), data.size());
        if (writeResult < 0 || writeResult != (int) data.size())
        {
            throw std::runtime_error(
                "Write failed '" + m_path + "'. Error: " + safeStrerror(errno));
        }
    }

private:
    const std::string m_path;
    int m_fd = -1;

    void openFile()
    {
        m_fd = ::open(m_path.data(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (m_fd < 0)
        {
            throw std::runtime_error(
                "Failed to open file '" + m_path + "'. Error: " + safeStrerror(errno));
        }
    }
};

void moveFile(const std::string& oldPath, const std::string& newPath)
{
    int result = ::rename(oldPath.data(), newPath.data());
    if (result < 0)
    {
        throw std::runtime_error(
            "Move failed old: '" + oldPath + "', new: '" + newPath
            + "'. Error: " + safeStrerror(errno));
    }
}

FileInfoList getFileInfoListImpl(
    const std::string& path, const std::optional<std::string>& ext = std::nullopt)
{
    FileInfoList result;
    DIR* dir = nullptr;
    struct dirent* entry = nullptr;

    if ((dir = ::opendir(path.data())) == nullptr)
        return result;

    while ((entry = ::readdir(dir)))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        const auto fileInfo = FileInfo(joinPath(path, entry->d_name));
        if (!ext || ext == fileInfo.extension)
            result.push_back(fileInfo);
    }

    ::closedir(dir);

    return result;
}

#endif // _WIN32

FileInfo::FileInfo(const std::string& path): fullPath(path)
{
    isDir = checkIfDir(path);
    size_t dotPos = std::string::npos;
    if (!isDir)
    {
        dotPos = fullPath.rfind('.');
        if (dotPos != std::string::npos)
            extension = fullPath.substr(dotPos);
    }

    const auto lastSeparatorPos = fullPath.rfind(kPathSeparator);
    if (lastSeparatorPos == std::string::npos || lastSeparatorPos >= fullPath.size() -1)
        throw std::logic_error("Invalid file info: no path separator found: '" + path + "'");

    if (dotPos != std::string::npos)
        baseName = fullPath.substr(lastSeparatorPos + 1, dotPos - lastSeparatorPos - 1);
    else
        baseName = fullPath.substr(lastSeparatorPos + 1);

    parentDir = fullPath.substr(0, lastSeparatorPos);
}

FileInfoList getFileInfoList(
    const std::string& path, const std::optional<std::string>& ext = std::nullopt)
{
    FileInfo base(path);
    if (!base.isDir)
        throw std::logic_error("Not a dir: '" + path + "')");

    return getFileInfoListImpl(path, ext);
}

struct MediaFileInfo: FileInfo
{
    MediaFileInfo() = default;
    MediaFileInfo(const FileInfo& fileInfo): FileInfo(fileInfo)
    {
        if (baseName.empty())
            throw std::logic_error("Invalid base name for file info '" + fullPath + "'");

        const auto parts = split(baseName, '_');
        streamIndex = std::stol(parts[0]);
        startTimeMs = std::stoll(parts[1]);
        if (parts.size() == 3)
            durationMs = std::stol(parts[2]);
    }

    int streamIndex = -1;
    int64_t durationMs;
    int64_t startTimeMs = -1;
};

std::string normalizeSeparators(std::string s)
{
    for (auto& c: s)
    {
        if (c == '\\')
            c= '/';
    }

    return s;
}

void renameWithDuration(const std::string& filePath, milliseconds duration)
{
    const auto fileInfo = FileInfo(filePath);
    if (filePath.size() < 4 || fileInfo.extension != kMediaFileExtenstion)
        throw std::logic_error("Invalid file path to rename " + filePath);

    const auto newPath = joinPath(
        fileInfo.parentDir,
        fileInfo.baseName + "_" + std::to_string(duration.count()) + kMediaFileExtenstion);

    moveFile(filePath.data(), newPath.data());
}

struct MediaFileHeader
{
    MediaFileHeader() = default;
    MediaFileHeader(const std::vector<uint8_t>& data, int* outPos)
    {
        *outPos = 0;
        const size_t codecInfoSize = read<size_t>(data, outPos);
        for (size_t i = 0; i < codecInfoSize; ++i)
        {
            const auto size = read<size_t>(data, outPos);
            const auto serializedCodecInfo = readBytes<std::string>(data, size, outPos);
            codecInfoList.push_back(nx::sdk::cloud_storage::CodecInfoData(serializedCodecInfo.data()));
        }

        const auto size = read<size_t>(data, outPos);
        opaqueMetadata = readBytes<std::string>(data, size, outPos);
    }

    MediaFileHeader(
        const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
        const char* opaqueMetadata)
        :
        opaqueMetadata(opaqueMetadata)
    {
        for (int i = 0; i < codecList->count(); ++i)
        {
            const auto entry = codecList->at(i);
            codecInfoList.push_back(
                nx::sdk::cloud_storage::CodecInfoData((const nx::sdk::cloud_storage::ICodecInfo*) entry.get()));
        }
    }

    std::vector<nx::sdk::cloud_storage::CodecInfoData> codecInfoList;
    std::string opaqueMetadata;

    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> result;
        size_t size = codecInfoList.size();
        append(&size, sizeof(size), &result);
        for (const auto& codecInfo: codecInfoList)
        {
            const auto serializedCodecInfo = codecInfo.to_json().dump();
            size = serializedCodecInfo.size();
            append(&size, sizeof(size), &result);
            append(serializedCodecInfo.data(), size, &result);
        }

        size = opaqueMetadata.size();
        append(&size, sizeof(size), &result);
        append(opaqueMetadata.data(), size, &result);

        return result;
    }
};

File::File(const std::string& path): m_fileImpl(std::make_unique<FileImpl>(path))
{
}

template<typename Container>
Container File::readAll() const
{
    return m_fileImpl->template readAll<Container>();
}

template<typename Container>
void File::writeAll(const Container& data, bool replaceContents)
{
    m_fileImpl->template writeAll<Container>(data, replaceContents);
}

File::~File()
{
}

WritableMediaFile::WritableMediaFile(
    const std::shared_ptr<std::mutex>& mutex,
    const std::string& path,
    std::chrono::milliseconds startTime,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
    const char* opaqueMetadata)
    :
    m_mutex(mutex),
    m_path(path),
    m_startTime(startTime)
{
    const auto header = MediaFileHeader(codecList, opaqueMetadata);
    m_buffer = header.serialize();
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_file = std::make_unique<File>(m_path);
}

void WritableMediaFile::writeMediaPacket(const nx::sdk::cloud_storage::IMediaDataPacket * packet)
{
    const auto data = mediaPacketDataToByteArray(
        nx::sdk::cloud_storage::MediaPacketData(packet, m_startTime));
    append(data.data(), data.size(), &m_buffer);
}

void WritableMediaFile::close(std::chrono::milliseconds duration)
{
    m_file->writeAll(m_buffer, /*replaceContents*/ false);
    m_file.reset();
    std::lock_guard<std::mutex> lock(*m_mutex);
    renameWithDuration(m_path, duration);
}

int WritableMediaFile::size() const
{
    return m_buffer.size();
}

WritableMediaFile::~WritableMediaFile()
{
}

int ReadableMediaFile::size() const
{
    return (int) m_fileData.size();
}

ReadableMediaFile::ReadableMediaFile(const std::string& path)
{
    if (!pathExists(path))
        throw std::runtime_error("Path does not exist: " + path);

    m_fileData = File(path).readAll<std::vector<uint8_t>>();
    m_header = std::make_unique<MediaFileHeader>(m_fileData, &m_begin);
    m_current = m_begin;
    const auto fileInfo = FileInfo(path);
    m_name = fileInfo.baseName + fileInfo.extension;
}

std::vector<nx::sdk::cloud_storage::CodecInfoData> ReadableMediaFile::codecInfo() const
{
    return m_header->codecInfoList;
}

std::string ReadableMediaFile::opaqueMetadata() const
{
    return m_header->opaqueMetadata;
}

std::optional<nx::sdk::cloud_storage::MediaPacketData> ReadableMediaFile::getNextPacket() const
{
    try
    {
        return mediaPacketData(m_fileData, &m_current);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::string ReadableMediaFile::name() const
{
    return m_name;
}

void ReadableMediaFile::seek(
    int64_t timestamp,
    bool findKeyFrame,
    int64_t* selectedPosition)
{
    int pos = m_begin;
    std::optional<nx::sdk::cloud_storage::MediaPacketData> lastKeyFramePacket;
    int lastKeyFramePos = -1;
    while (true)
    {
        try
        {
            const auto packet = mediaPacketData(m_fileData, &pos);
            if ((int64_t) packet.timestampUs > timestamp)
            {
                if (!findKeyFrame)
                {
                    m_current = pos;
                    *selectedPosition = packet.timestampUs;
                    return;
                }

                if (lastKeyFramePacket)
                {
                    m_current = lastKeyFramePos;
                    *selectedPosition = lastKeyFramePacket->timestampUs;
                    return;
                }
            }

            if (packet.isKeyFrame & nxcip::MediaDataPacket::fKeyPacket)
            {
                lastKeyFramePacket = packet;
                lastKeyFramePos = pos;
            }
        }
        catch (...)
        {
            if (findKeyFrame && lastKeyFramePacket)
            {
                m_current = lastKeyFramePos;
                *selectedPosition = lastKeyFramePacket->timestampUs;
                return;
            }

            throw;
        }
    }
}

ReadableMediaFile::~ReadableMediaFile()
{
}

nx::sdk::cloud_storage::DeviceDescription descriptionFromDeviceInfo(
    const nx::sdk::IDeviceInfo* info)
{
    using namespace nx::sdk::cloud_storage;
    DeviceDescription d;
    d.parameters.push_back(DeviceParameter{ "id", info->id() });
    d.parameters.push_back(DeviceParameter{ "channelNumber", std::to_string(info->channelNumber()) });
    d.parameters.push_back(DeviceParameter{ "firmware", info->firmware() });
    d.parameters.push_back(DeviceParameter{ "logicalId", info->logicalId() });
    d.parameters.push_back(DeviceParameter{ "login", info->login() });
    d.parameters.push_back(DeviceParameter{ "model", info->model() });
    d.parameters.push_back(DeviceParameter{ "name", info->name() });
    d.parameters.push_back(DeviceParameter{ "password", info->password() });
    d.parameters.push_back(DeviceParameter{ "sharedId", info->sharedId() });
    d.parameters.push_back(DeviceParameter{ "url", info->url() });
    d.parameters.push_back(DeviceParameter{ "vendor", info->vendor() });

    return d;
}

std::optional<std::string> deviceId(const nx::sdk::cloud_storage::DeviceDescription& description)
{
    const auto idIt = std::find_if(
        description.parameters.cbegin(), description.parameters.cend(),
        [](const auto& p) { return p.name == "id"; });

    if (idIt == description.parameters.cend())
        return std::nullopt;

    return idIt->value;
}

int toStreamIndex(nxcip::MediaStreamQuality quality)
{
    switch (quality)
    {
        case nxcip::msqDefault:
        case nxcip::msqHigh:
            return 0;
        case nxcip::msqLow:
            return 1;
    }

    throw std::logic_error("Unexpected quality");
    return -1;
}

std::string deviceId(nx::sdk::cloud_storage::IDeviceAgent* device)
{
    return nx::sdk::Ptr(device->deviceInfo().value())->id();
}

std::string toString(sdk::cloud_storage::MetadataType metadataType)
{
    using MetadataType = sdk::cloud_storage::MetadataType;
    switch (metadataType)
    {
        case MetadataType::analytics: return "analytics";
        case MetadataType::motion: return "motion";
        case MetadataType::bookmark: return "bookmark";
    }

    throw std::logic_error("Unexpected metadataType");
}

DataManager::DataManager(const std::string& workDir):
    m_workDir(normalizeSeparators(workDir))
{
    const auto kStubBucketToId = std::make_pair("bucket_24", 24);
    m_bucketUrlToId = kStubBucketToId;
    size_t last = m_workDir.size() - 1;
    while (last >= 0 && (m_workDir.at(last) == '/' || m_workDir.at(last) == '\\'))
        last--;

    if (last != m_workDir.size() - 1)
        m_workDir = m_workDir.substr(0, last + 1);

    NX_OUTPUT << __func__ << ": Work dir: '" << m_workDir << "'";
    ensureDir(joinPath(m_workDir, kImageFolderName));
    using namespace nx::sdk::cloud_storage;
    m_cloudAnalytics = load<ObjectTrack>(joinPath(m_workDir, kAnalyticsFileName));
    m_cloudMotion = load<Motion>(joinPath(m_workDir, kMotionFileName));
    m_cloudBookmarks = load<Bookmark>(joinPath(m_workDir, kBookmarkFileName));
    m_workThread = std::thread([this](){ run(); });
}

void DataManager::run()
{
    while (!m_needToStop)
    {
        std::unique_lock<std::mutex> lock(*m_mutex);
        bool needToWait =
            !*needToFlush<nx::sdk::cloud_storage::Bookmark>()
            && dataQueue<nx::sdk::cloud_storage::Bookmark>()->size() < kMaxSendBufferSize
            && !*needToFlush<nx::sdk::cloud_storage::Motion>()
            && dataQueue<nx::sdk::cloud_storage::Motion>()->size() < kMaxSendBufferSize
            && !*needToFlush<nx::sdk::cloud_storage::ObjectTrack>()
            && dataQueue<nx::sdk::cloud_storage::ObjectTrack>()->size() < kMaxSendBufferSize;

        if (needToWait)
            m_cond.wait(lock);

        if (m_needToStop)
            return;

        processQueue(m_analyticsToSend, lock);
        processQueue(m_motionToSend, lock);
        processQueue(m_bookmarksToSend, lock);
    }
}

template<typename Data>
std::queue<Data>* DataManager::dataQueue()
{
    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Bookmark>)
        return &m_bookmarksToSend;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Motion>)
        return &m_motionToSend;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::ObjectTrack>)
        return &m_analyticsToSend;
}

template<typename Data>
bool*  DataManager::needToFlush()
{
    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Bookmark>)
        return &m_needToFlushBookmarks;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Motion>)
        return &m_needToFlushMotion;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::ObjectTrack>)
        return &m_needToFlushAnalytics;
}

template<typename Data>
nx::sdk::cloud_storage::MetadataType DataManager::metadataType() const
{
    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Bookmark>)
        return nx::sdk::cloud_storage::MetadataType::bookmark;;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Motion>)
        return nx::sdk::cloud_storage::MetadataType::motion;;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::ObjectTrack>)
        return nx::sdk::cloud_storage::MetadataType::analytics;;
}

template<typename Data>
std::vector<Data>* DataManager::cloudData()
{
    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Bookmark>)
        return &m_cloudBookmarks;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::Motion>)
        return &m_cloudMotion;

    if constexpr (std::is_same_v<Data, nx::sdk::cloud_storage::ObjectTrack>)
        return &m_cloudAnalytics;
}

template<typename Data>
void DataManager::processQueue(std::queue<Data>& dataQueue, std::unique_lock<std::mutex>& lock)
{
    auto* pCloudData = cloudData<Data>();
    if (!*needToFlush<Data>() && dataQueue.size() < kMaxSendBufferSize)
        return;

    while (!dataQueue.empty())
    {
        const auto& data = dataQueue.front();
        if constexpr (std::is_same_v<nx::sdk::cloud_storage::Bookmark, Data>)
        {
            for (auto it = pCloudData->begin(); it != pCloudData->end();)
            {
                if (it->id == data.id)
                    it = pCloudData->erase(it);
                else
                    ++it;
            }
        }
        pCloudData->push_back(data);
        dataQueue.pop();
    }

    *needToFlush<Data>() = false;
    lock.unlock();
    m_saveHandler(metadataType<Data>(), nx::sdk::ErrorCode::noError);
    lock.lock();
}

void DataManager::setSaveHandler(SaveHandler handler)
{
    m_saveHandler = std::move(handler);
}

void DataManager::flushMetadata(nx::sdk::cloud_storage::MetadataType type)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    switch (type)
    {
        case nx::sdk::cloud_storage::MetadataType::analytics:
            m_needToFlushAnalytics = true;
            break;
        case nx::sdk::cloud_storage::MetadataType::motion:
            m_needToFlushMotion = true;
            break;
        case nx::sdk::cloud_storage::MetadataType::bookmark:
            m_needToFlushBookmarks = true;
            break;
    }

    m_cond.notify_one();
}

DataManager::~DataManager()
{
    m_needToStop = true;
    m_cond.notify_one();
    m_workThread.join();
    flushToFile(m_cloudAnalytics, joinPath(m_workDir, kAnalyticsFileName));
    flushToFile(m_cloudMotion, joinPath(m_workDir, kMotionFileName));
    flushToFile(m_cloudBookmarks, joinPath(m_workDir, kBookmarkFileName));
}

nx::sdk::ErrorCode DataManager::saveBookmark(const nx::sdk::cloud_storage::Bookmark& data)
{
    return saveMetadataImpl(data);
}

void DataManager::deleteBookmark(const char* id)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    const auto fileName = joinPath(m_workDir, kBookmarkFileName);
    auto bookmarks = allObjectsNoLock<nx::sdk::cloud_storage::Bookmark>(fileName);
    bookmarks.erase(
        std::remove_if(bookmarks.begin(), bookmarks.end(),
        [id](const auto& entry) { return entry.id == id; }), bookmarks.end());

    File(fileName).writeAll(nx::kit::Json(bookmarks).dump(), /*replaceContents*/ true);
}

template<typename Data>
void DataManager::saveObject(const Data& data, const std::string& fileName)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    File(fileName).writeAll(nx::kit::Json(data).dump(), /*replaceContents*/ true);
}

std::vector<nx::kit::Json> DataManager::loadListNoLock(const std::string& fileName) const
{
    const auto fileData = File(fileName).readAll<std::string>();
    nx::kit::Json json;
    std::string jsonError;
    if (!fileData.empty())
    {
        json = nx::kit::Json::parse(fileData.data(), jsonError);
        if (json.is_null() || !jsonError.empty())
            throw std::runtime_error("JSON parse error: " + jsonError);

        if (!json.is_array())
            throw std::runtime_error("JSON parse error: not array");
    }
    else
    {
        json = nx::kit::Json::parse("[]", jsonError);
    }

    return json.array_items();
}

template<typename DataList>
void DataManager::flushToFile(const DataList& data, const std::string& fileName)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    auto file = File(fileName);
    std::vector<nx::kit::Json> jsonDataList;
    for (const auto& entry: data)
        jsonDataList.push_back(nx::kit::Json(entry));

    file.writeAll(nx::kit::Json(jsonDataList).dump(), /*replaceContents*/ true);
}

template<typename Data>
std::vector<Data> DataManager::load(const std::string& fileName) const
{
    std::vector<nx::kit::Json> jsonDataList;
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(*m_mutex));
        jsonDataList = loadListNoLock(fileName);
    }

    std::vector<Data> result;
    for (const auto& jsonEntry: jsonDataList)
        result.push_back(Data(jsonEntry));

    return result;
}

template<typename Data>
std::optional<Data> DataManager::oneObject(const std::string& fileName) const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(*m_mutex));
    if (!pathExists(fileName))
        return std::nullopt;

    auto file = File(fileName);
    const auto fileData = file.readAll<std::string>();
    nx::kit::Json json;
    std::string jsonError;
    if (fileData.empty())
        return std::nullopt;

    json = nx::kit::Json::parse(fileData.data(), jsonError);
    if (json.is_null() || !jsonError.empty())
        throw std::runtime_error("JSON parse error: " + jsonError);

    return Data(json);
}

template<typename Data>
std::vector<Data> DataManager::allObjects(const std::string& fileName) const
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    return DataManager::allObjectsNoLock<Data>(fileName);
}

template<typename Data>
std::vector<Data> DataManager::allObjectsNoLock(const std::string& fileName) const
{
    if (!pathExists(fileName))
        return std::vector<Data>();

    auto file = File(fileName);
    const auto fileData = file.readAll<std::string>();
    nx::kit::Json json;
    std::string jsonError;
    if (fileData.empty())
        return std::vector<Data>();

    json = nx::kit::Json::parse(fileData.data(), jsonError);
    if (json.is_null() || !jsonError.empty())
        throw std::runtime_error("JSON parse error: " + jsonError);

    if (!json.is_array())
        throw std::runtime_error("JSON parse error: not array");

    std::vector<Data> result;
    for (const auto& entryJson: json.array_items())
        result.push_back(Data(entryJson));

    return result;
}

std::string DataManager::queryBookmarks(const nx::sdk::cloud_storage::BookmarkFilter& filter) const
{
    using namespace nx::sdk::cloud_storage;
    std::vector<Bookmark> result;
    std::lock_guard<std::mutex> lock(*m_mutex);
    for (const auto& candidate: m_cloudBookmarks)
    {
        if (bookmarkMatches(candidate, filter))
            result.push_back(candidate);
    }

    sortAndLimitBookmarks(filter, &result);
    return dumpObjects(result);
}

nx::sdk::ErrorCode DataManager::saveMotion(const nx::sdk::cloud_storage::Motion& data)
{
    return saveMetadataImpl(data);
}

template<typename Data>
nx::sdk::ErrorCode DataManager::saveMetadataImpl(const Data& data)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    auto* pQueue = dataQueue<Data>();
    pQueue->push(data);
    if (pQueue->size() >= kMaxSendBufferSize)
    {
        m_cond.notify_one();
        return nx::sdk::ErrorCode::inProgress;
    }

    return nx::sdk::ErrorCode::needMoreData;
}

std::string DataManager::queryMotion(const nx::sdk::cloud_storage::MotionFilter& filter) const
{
    using namespace nx::sdk::cloud_storage;
    std::vector<TimePeriod> filtered;
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (const auto& candidate: m_cloudMotion)
        {
            if (motionMaches(candidate, filter))
                filtered.push_back({candidate.startTimeMs, candidate.durationMs});
        }
    }
    if (filter.order == nx::sdk::cloud_storage::SortOrder::descending)
        std::reverse(filtered.begin(), filtered.end());
    if (filter.limit && filtered.size() > filter.limit)
        filtered.resize(*filter.limit);

    using namespace nx::sdk::cloud_storage;
    auto result = DeltaTimePeriods::pack(filtered, filter.order);
    return result.to_json().dump();
}

nx::sdk::ErrorCode DataManager::saveObjectTrack(const nx::sdk::cloud_storage::ObjectTrack& data)
{
    return saveMetadataImpl(data);
}

std::string DataManager::queryAnalytics(
    const nx::sdk::cloud_storage::AnalyticsFilter& filter) const
{
    std::vector<nx::sdk::cloud_storage::ObjectTrack> result;
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (const auto& candidate: m_cloudAnalytics)
        {
            if (objectTrackMatches(candidate, filter))
                result.push_back(candidate);
        }
    }

    return dumpObjects(result);
}

void ArchiveIndex::sort()
{
    for (auto& deviceArchiveIndex: deviceArchiveIndexList)
    {
        for (int i: {0, 1})
        {
            auto& streamChunks = deviceArchiveIndex.chunksPerStream;
            auto streamChunksIt = streamChunks.find(i);
            if (streamChunksIt != streamChunks.cend())
            {
                auto& chunks = *streamChunksIt;
                std::sort(chunks.second.begin(), chunks.second.end());
            }
        }
    }
}

void roundToGrid(std::vector<nx::sdk::cloud_storage::TimePeriod>& periods, int intervalMs = 5000)
{
    using namespace std::chrono;
    for (auto& period: periods)
    {
        auto endTime = period.duration.count() > 0
            ? period.startTimestamp + period.duration : milliseconds(0);
        const int64_t dt = period.startTimestamp.count() % intervalMs;
        if (dt > 0)
            period.startTimestamp -= milliseconds(dt);
        if (endTime.count() > 0)
        {
            const int64_t dt = endTime.count() % intervalMs;
            if (dt > 0)
                endTime += milliseconds(intervalMs - dt);
            period.duration = endTime - period.startTimestamp;
        }
    }
}

std::string DataManager::queryAnalyticsPeriods(
    const nx::sdk::cloud_storage::AnalyticsFilter& filter) const
{
    using namespace nx::sdk::cloud_storage;
    std::vector<TimePeriod> result;
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (const auto& candidate: m_cloudAnalytics)
        {
            if (!objectTrackMatches(candidate, filter))
                continue;

            const auto startTime =
                duration_cast<milliseconds>(candidate.firstAppearanceTimestamp);
            const auto endTime =
                duration_cast<milliseconds>(candidate.lastAppearanceTimestamp);
            result.push_back({startTime, endTime - startTime});
        }
    }
    if (filter.order == nx::sdk::cloud_storage::SortOrder::descending)
        std::reverse(result.begin(), result.end());
    if (filter.maxObjectTracksToSelect && result.size() > filter.maxObjectTracksToSelect)
        result.resize(*filter.maxObjectTracksToSelect);
    roundToGrid(result, 5000);
    auto compressed = DeltaTimePeriods::pack(result, filter.order);
    return compressed.to_json().dump();
}

void DataManager::addDevice(const nx::sdk::cloud_storage::DeviceDescription& deviceDescription)
{
    const auto deviceDir = joinPath(
        joinPath(m_workDir, m_bucketUrlToId.first), *deviceDescription.deviceId());
    ensureDir(deviceDir);
    saveObject(deviceDescription, joinPath(deviceDir, kDeviceDescriptionFileName));
}

std::string DataManager::devicePath(const std::string& deviceId) const
{
    return joinPath(joinPath(m_workDir, m_bucketUrlToId.first), deviceId);
}

ArchiveIndex DataManager::getArchive(
    std::optional<int64_t> startTime) const
{
    ArchiveIndex result;
    for (const auto& bucketDir: getFileInfoList(m_workDir))
    {
        if (!bucketDir.isDir)
            continue;

        if (bucketDir.baseName != m_bucketUrlToId.first)
            continue;

        for (const auto& deviceDir: getFileInfoList(bucketDir.fullPath))
        {
            if (!deviceDir.isDir)
                continue;

            using namespace nx::sdk::cloud_storage;
            DeviceArchiveIndex deviceArchiveIndex;
            const auto maybeDescription =
                oneObject<DeviceDescription>(joinPath(deviceDir.fullPath, kDeviceDescriptionFileName));

            if (!maybeDescription)
                continue;

            deviceArchiveIndex.deviceDescription = *maybeDescription;
            std::lock_guard<std::mutex> lock(*m_mutex);
            for (const auto& file: getFileInfoList(deviceDir.fullPath, kMediaFileExtenstion))
            {
                auto mediaFileInfo = MediaFileInfo(file);
                if (!mediaFileInfo.durationMs
                    || (startTime && mediaFileInfo.startTimeMs <= startTime))
                {
                    continue;
                }

                deviceArchiveIndex.chunksPerStream[mediaFileInfo.streamIndex].push_back(MediaChunk{
                    mediaFileInfo.startTimeMs, /* startPoint */
                    mediaFileInfo.durationMs, /* durationMs */
                    m_bucketUrlToId.first.data() /* bucketId */});
            }

            result.deviceArchiveIndexList.push_back(deviceArchiveIndex);
        }
    }

    result.sort();
    return result;
}

std::pair<std::string, int> DataManager::bucketUrlAndId() const
{
    return m_bucketUrlToId;
}

std::unique_ptr<WritableMediaFile> DataManager::writableMediaFile(
    const std::string& deviceId,
    int streamIndex,
    std::chrono::milliseconds timestamp,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
    const char* opaqueMetadata) const
{
    ensureDir(joinPath(joinPath(m_workDir, bucketUrlAndId().first), deviceId));
    return std::make_unique<WritableMediaFile>(
        m_mutex, mediaFilePath(bucketUrlAndId().first, deviceId, streamIndex, timestamp), timestamp, codecList,
        opaqueMetadata);
}

std::unique_ptr<ReadableMediaFile> DataManager::readableMediaFile(
    const std::string& bucketUrl,
    const std::string& deviceId,
    int streamIndex,
    int64_t startTimeMs,
    int64_t durationMs) const
{
    return std::make_unique<ReadableMediaFile>(
        mediaFilePath(bucketUrl, deviceId, streamIndex, startTimeMs, durationMs));
}

std::string DataManager::mediaFilePath(
    const std::string& bucketUrl,
    const std::string& deviceId,
    int streamIndex,
    std::chrono::milliseconds timestamp) const
{
    const auto fileName = std::to_string(streamIndex) + "_"
        + std::to_string(timestamp.count()) + kMediaFileExtenstion;

    return joinPath(m_workDir, joinPath(joinPath(bucketUrl, deviceId), fileName));
}

std::string DataManager::mediaFilePath(
    const std::string& bucketUrl,
    const std::string& deviceId,
    int streamIndex,
    int64_t startTimeMs,
    int64_t durationMs) const
{
    const auto durationStr = durationMs > 0 ? "_" + std::to_string(durationMs) : "";
    const auto fileName = std::to_string(streamIndex) + "_"
        + std::to_string(startTimeMs) + durationStr + kMediaFileExtenstion;

    return joinPath(m_workDir, joinPath(joinPath(bucketUrl, deviceId), fileName));
}

void DataManager::saveTrackImage(
    const char* data,
    nx::sdk::cloud_storage::TrackImageType type)
{
    const auto image = nx::sdk::cloud_storage::Image(data);
    File f(pathToImage(m_workDir, image.objectTrackId, type));
    f.writeAll(std::string(data), /*replaceContents*/ true);
}

std::string DataManager::fetchTrackImage(
    const char* objectTrackId,
    nx::sdk::cloud_storage::TrackImageType type)
{
    const auto filePath = pathToImage(m_workDir, objectTrackId, type);
    if (!pathExists(filePath))
        throw std::runtime_error("Track image file " + std::string(objectTrackId) + " not found");

    File f(filePath);
    return f.readAll<std::string>();
}

} //namespace nx::vms_server_plugins::cloud_storage::stub
