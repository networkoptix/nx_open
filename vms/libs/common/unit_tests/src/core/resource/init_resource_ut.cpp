#include <gtest/gtest.h>

#include <nx/core/resource/device_mock.h>
#include <nx/utils/thread/wait_condition.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/core/access/access_types.h>

namespace nx::core::resource::tesdt {

class ResourceMock : public DeviceMock
{
public:
    virtual CameraDiagnostics::Result initInternal() override
    {
        QnMutexLocker lock(&m_mutex);
        m_inInit = true;
        while (m_initBlocked)
            m_waitCond.wait(&m_mutex);

        ++m_initAttempts;
        m_inInit = false;
        return CameraDiagnostics::NoErrorResult();
    }

    void waitForInitStarted()
    {
        while (!m_inInit)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void blockInit()
    {
        QnMutexLocker lock(&m_mutex);
        m_initBlocked = true;
        m_waitCond.wakeAll();
    }

    void unblockInit()
    {
        QnMutexLocker lock(&m_mutex);
        m_initBlocked = false;
        m_waitCond.wakeAll();
    }

    int initAttempts()
    {
        while (isInitializationInProgress())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return m_initAttempts;
    }

protected:
    int m_initAttempts = 0;
    std::atomic<int> m_inInit = false;
    bool m_initBlocked = false;
    QnMutex m_mutex;
    QnWaitCondition m_waitCond;
};

TEST(InitResource, main)
{
    QnCommonModule commonModule(false, nx::core::access::Mode::direct);
    QnResourcePool pool;
    auto resource = QnSharedResourcePointer<ResourceMock>(new ResourceMock());
    resource->setIdUnsafe(QnUuid::createUuid());
    resource->setResourcePool(&pool);
    resource->setCommonModule(&commonModule);

    resource->initAsync();
    while (!resource->isInitialized())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_EQ(1, resource->initAttempts());

    resource->setStatus(Qn::Offline); //< reset initialization.
    resource->blockInit();
    resource->initAsync();
    resource->waitForInitStarted();
    for (int i = 0; i < 5; ++i)
        resource->reinitAsync(); //< start reinit during initialization.
    resource->unblockInit();

    ASSERT_EQ(3, resource->initAttempts());

    resource->blockInit();
    resource->reinitAsync(); //< start reinit without initialization
    resource->waitForInitStarted();
    resource->unblockInit();
    ASSERT_EQ(4, resource->initAttempts());
}

} // namespace nx::core::resource::tesdt
