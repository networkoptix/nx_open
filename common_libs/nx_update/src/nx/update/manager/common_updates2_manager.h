#pragma once

#include <common/common_module_aware.h>
#include <nx/update/manager/detail/updates2_manager_base.h>

namespace nx {
namespace update {

class NX_UPDATE_API CommonUpdateManager: public QnCommonModuleAware
{
public:
    CommonUpdateManager(QnCommonModule* commonModule);

    // Call this to connect to all needed signals
    void connectToSignals();

protected:
    //virtual QnUuid peerId() const override;

private:
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual installer::detail::AbstractUpdates2Installer* installer() = 0;

};

} // namespace update
} // namespace nx
