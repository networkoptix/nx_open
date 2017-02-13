#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <future>

#include <nx/utils/std/thread.h>

#include "media_server_process.h"
#include "platform/platform_abstraction.h"
#include "utils.h"

class MediaServerProcess;
class QnPlatformAbstraction;

namespace nx {
namespace ut {
namespace utils {

using MediaServerTestFuncTypeList = std::vector<std::function<void()>>;

/**
 * This class is deprecated!
 * Please use MediaServerLauncher instead.
 */

class MediaServerHelper
{
public:
    MediaServerHelper(const MediaServerTestFuncTypeList& testList);
    ~MediaServerHelper();
    void start();

private:
    std::unique_ptr<MediaServerProcess> m_serverProcess;
    MediaServerTestFuncTypeList m_testList;
    nx::ut::utils::WorkDirResource m_workDirResource;
    nx::utils::thread m_thread;
    nx::utils::promise<void> m_testReadyPromise;
    nx::utils::future<void> m_testsReadyFuture;
};
}
}
}
