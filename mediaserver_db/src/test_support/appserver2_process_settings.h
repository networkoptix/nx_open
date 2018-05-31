#pragma once

#include <chrono>

#include <nx/network/socket_common.h>
#include <nx/utils/settings.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

namespace ec2 {
namespace conf {

struct CloudIntegration
{
    std::chrono::milliseconds delayBeforeSettingMasterFlag;
    nx::utils::Url cloudDbUrl;

    CloudIntegration();
};

class Settings
{
public:
    Settings();

    void load(int argc, char** argv);
    bool showHelp();

    QString dbFilePath() const;
    bool isP2pMode() const;
    int moduleInstance() const;
    nx::network::SocketAddress endpoint() const;
    QnUuid moduleGuid() const;
    bool isAuthDisabled() const;

    const CloudIntegration& cloudIntegration() const;

private:
    QnSettings m_settings;
    bool m_showHelp;
    CloudIntegration m_cloudIntegration;

    void loadCloudIntegration();
};

} // namespace conf
} // namespace ec2
