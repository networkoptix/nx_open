#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <future>

#include "media_server_process.h"
#include "platform/platform_abstraction.h"

class MediaServerProcess;
class QnPlatformAbstraction;

namespace nx {
namespace ut {
namespace utils {

using MediaServerTestFuncTypeList = std::vector<std::function<void()>>;

// starts whole mediaserver for you
// supply list of what you want to test as a constructor parameter
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
    std::unique_ptr<QnPlatformAbstraction> m_platform;
    std::thread m_thread;
    std::promise<void> m_testReadyPromise;
    std::future<void> m_testsReadyFuture;
};
}
}
}