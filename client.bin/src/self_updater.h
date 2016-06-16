#pragma once

#include <nx/utils/software_version.h>

struct QnStartupParameters;

namespace nx {
namespace vms {
namespace client {

/** Class for updating client installation if needed. */
class SelfUpdater
{
public:
    SelfUpdater(const QnStartupParameters& startupParams);

private:
    bool registerUriHandler();
    bool updateClientInstallation();

private:
    nx::utils::SoftwareVersion m_clientVersion;
    bool m_exitNow;
};

}
}
}