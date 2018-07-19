#pragma once

#include <nx/update/common_update_manager.h>

namespace nx {
namespace mediaserver {

class ServerUpdateManager: public CommonUpdateManager
{
public:
    ServerUpdateManager(QnCommonModule* commonModule);

private:
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
};

} // namespace mediaserver
} // namespace nx