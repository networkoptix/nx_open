#pragma once

#include <string>
#include <functional>
#include <boost/optional.hpp>

namespace nx {

class SystemCommands
{
public:
    static const char* const kDomainSocket;

    enum class UnmountCode
    {
        ok,
        busy,
        notExists,
        noPermissions,
        notMounted
    };

    enum class MountCode
    {
        ok,
        wrongCredentials,
        otherError
    };


    struct Stats
    {
        enum class FileType
        {
            regular,
            directory,
            other
        };

        bool exists = false;
        FileType type = FileType::other;
        int64_t size = -1;
    };

    /** Mounts NAS from url to directory for real UID and GID. */
    MountCode mount(
        const std::string& url,
        const std::string& directory,
        const boost::optional<std::string>& username,
        const boost::optional<std::string>& password);

    /** Unounts NAS from directory. */
    UnmountCode unmount(const std::string& directory);

    /** Changes path ownership to real UID and GID. */
    bool changeOwner(const std::string& path, int uid, int gid, bool isRecursive);

    /** Creates directory and gives ownership to real UID and GID. */
    bool makeDirectory(const std::string& directoryPath);

    /** Removes recursively given path. */
    bool removePath(const std::string& path);

    /** Renames (moves) oldPath to the newPath. */
    bool rename(const std::string& oldPath, const std::string& newPath);

    /** Returns file descriptor for the given path. */
    int open(const std::string& path, int mode);

    /** Returns free space on the device which given path belongs to */
    int64_t freeSpace(const std::string& path);

    /** Returns total space on the device which given path belongs to */
    int64_t totalSpace(const std::string& path);

    /** Returns if the given path exists. */
    bool isPathExists(const std::string& path);

    /** Returns CSV list of file entries - "fileName,fileSize,isDir" */
    std::string serializedFileList(const std::string& path);

    /** Returns stats struct */
    Stats stat(const std::string& path);

    /** Returns file size. */
    int64_t fileSize(const std::string& path);

    /** Gets device path by file system path */
    std::string devicePath(const std::string& path);

    std::string serializedDmiInfo();

    std::string lastError() const;

    static const char* unmountCodeToString(UnmountCode code)
    {
        switch (code)
        {
            case UnmountCode::ok: return "ok";
            case UnmountCode::busy: return "resource is busy";
            case UnmountCode::notExists: return "path not exists";
            case UnmountCode::noPermissions: return "no permissions";
            case UnmountCode::notMounted: return "not mounted";
        }

        return "";
    }

    static const char* mountCodeToString(MountCode code)
    {
        switch (code)
        {
            case MountCode::ok: return "ok";
            case MountCode::wrongCredentials: return "wrong credentials";
            case MountCode::otherError: return "error";
        }

        return "";
    }

private:
    std::string m_lastError;

    bool checkMountPermissions(const std::string& directory);
    bool checkOwnerPermissions(const std::string& path);
    bool execute(const std::string& command, std::function<void(const char*)> outputAction = nullptr);
};

} // namespace nx
