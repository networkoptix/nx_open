#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <future>

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
    std::thread m_thread;
    std::promise<void> m_testReadyPromise;
    std::future<void> m_testsReadyFuture;
};
}
}
}
