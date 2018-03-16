#include <string>
#include <boost/optional.hpp>

namespace nx {
namespace root_tool {

class SystemCommands
{
public:
    /** Mounts NAS from url to directory for real UID and GID. */
    bool mount(const std::string& url, const std::string& directory,
        const boost::optional<std::string>& username, const boost::optional<std::string>& password);

    /** Unounts NAS from directory. */
    bool unmount(const std::string& directory);

    /** Changes path ownership to real UID and GID. */
    bool changeOwner(const std::string& path);

    /** Touches a file and gives ownership to real UID and GID. */
    bool touchFile(const std::string& filePath);

    /** Creates directory and gives ownership to real UID and GID. */
    bool makeDirectory(const std::string& directoryPath);

    /** Installs deb package to system. */
    bool install(const std::string& debPackage);

    /** Prints real and effective UIDs and GIDs. */
    void showIds();

    /** Set real UID and GID to effective (some shell utils require that). */
    bool setupIds();

    std::string lastError() const;

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
    bool execute(const std::string& command);
    CheckOwnerResult checkCurrentOwner(const std::string& url);
};

} // namespace root_tool
} // namespace nx
