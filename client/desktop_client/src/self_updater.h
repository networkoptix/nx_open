#pragma once

#include <nx/utils/software_version.h>

struct QnStartupParameters;

namespace nx {
namespace vms {
namespace client {

/**
    Class for updating client installation if needed.

    There are some operations that should be done during self-update.
    Several of them require administrator privileges on different os.

    | Operation           | Windows | Linux   | MacOS   |
    | ------------------- | ------- | ------- | ------- |
    | Register URI        | admin   | user    | ???     |
    | Update applaucher   | user    | user    | user    |
    | Update minilauncher | admin   | admin   | ???     |
    | ------------------- | ------- | ------- | ------- |

    If client is run under administrator (with self-update key passed),
    we should do all required operations and then exit immediately.
*/
class SelfUpdater
{
public:
    SelfUpdater(const QnStartupParameters& startupParams);

private:
    enum class Operation
    {
        RegisterUriHandler,
        UpdateApplauncher,
        UpdateMinilauncher
    };

    enum class Result
    {
        Success,
        AdminRequired,
        Failure
    };

    bool registerUriHandler();
    bool updateApplauncher();
    bool updateMinilauncher();

    nx::utils::SoftwareVersion getVersionFromFile(const QString& filename) const;
    bool saveVersionToFile(const QString& filename, const nx::utils::SoftwareVersion& version) const;

    Result osCheck(Operation operation, bool result);
    void launchWithAdminPermissions();

    bool isMinilaucherUpdated(const QString& installRoot) const;
    bool runMinilaucher() const;
    bool updateMinilauncherInDir(const QString& installRoot);

    /** Get all paths where client might be installed. */
    QStringList getClientInstallRoots() const;
private:
    nx::utils::SoftwareVersion m_clientVersion;
};

}
}
}
