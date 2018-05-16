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
        noPermissions
    };

    enum class MountCode
    {
        ok,
        wrongCredentials,
        otherError
    };

    /** Mounts NAS from url to directory for real UID and GID. */
    MountCode mount(
        const std::string& url,
        const std::string& directory,
        const boost::optional<std::string>& username,
        const boost::optional<std::string>& password,
        bool reportViaSocket,
        int socketPostfix = -1);

    /** Unounts NAS from directory. */
    UnmountCode unmount(const std::string& directory, bool reportViaSocket, int socketPostfix = -1);

    /** Changes path ownership to real UID and GID. */
    bool changeOwner(const std::string& path);

    /** Touches a file and gives ownership to real UID and GID. */
    bool touchFile(const std::string& filePath);

    /** Creates directory and gives ownership to real UID and GID. */
    bool makeDirectory(const std::string& directoryPath);

    /** Removes recursively given path. */
    bool removePath(const std::string& path);

    /** Renames (moves) oldPath to the newPath. */
    bool rename(const std::string& oldPath, const std::string& newPath);

    /** Returns file descriptor for the given path. */
    int open(const std::string& path, int mode, bool reportViaSocket, int socketPostfix = -1);

    /** Returns free space on the device which given path belongs to */
    int64_t freeSpace(const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    /** Returns total space on the device which given path belongs to */
    int64_t totalSpace(const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    /** Returns the given path exists */
    bool isPathExists(const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    /** Returns CSV list of file entries - "fileName,fileSize,isDir" */
    std::string serializedFileList(
        const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    /** Returns file size. */
    int64_t fileSize(const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    /** Gets device path by file system path */
    std::string devicePath(const std::string& path, bool reportViaSocket, int socketPostfix = -1);

    bool kill(int pid);

    std::string serializedDmiInfo(bool reportViaSocket, int socketPostfix = -1);

    /** Installs deb package to system. */
    bool install(const std::string& debPackage);

    /** Prints real and effective UIDs and GIDs. */
    void showIds();

    /** Set real UID and GID to effective (some shell utils require that). */
    bool setupIds();

    std::string lastError() const;

    static const char* unmountCodeToString(UnmountCode code)
    {
        switch (code)
        {
            case UnmountCode::ok: return "ok";
            case UnmountCode::busy: return "resource is busy";
            case UnmountCode::notExists: return "path not exists";
            case UnmountCode::noPermissions: return "no permissions";
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
    enum class CheckOwnerResult
    {
        real,
        other,
        failed,
    };

    std::string m_lastError;

    bool checkMountPermissions(const std::string& directory);
    bool checkOwnerPermissions(const std::string& path);
    bool execute(
        const std::string& command, std::function<void(const char*)> outputAction = nullptr);
    CheckOwnerResult checkCurrentOwner(const std::string& url);
};

} // namespace nx
