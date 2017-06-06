#include <gtest/gtest.h>
#include <nx/utils/std/future.h>
#include <chrono>
#include <managers/persistent_timer_manager.h>

namespace nx {
namespace cdb {
namespace test {

class PersistentTimerManager : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        readyFuture = readyPromise.get_future();
    }

    nx::cdb::PersistentTimerManager manager;
    nx::utils::future<void> readyFuture;
    nx::utils::promise<void> readyPromise;
};


class ManagerUser: public nx::cdb::AbstractPersistentTimerEventReceiver
{
public:
    static const QnUuid userTaskGuid;

    ManagerUser(nx::cdb::PersistentTimerManager* manager, nx::utils::promise* readyPromise):
        m_manager(manager)
    {
        manager->registerEventReceiver(userTaskGuid, this);
    }

    virtual void persistentTimerFired(const PersistentParamsMap& params) override
    {
        ASSERT_NE(params.find("key1"), params.cend());
        ASSERT_NE(params.find("key2"), params.cend());

        ++m_fired;
        if (m_shouldUnsubscribe)
        {
            m_manager->unsubscribe(userTaskGuid);
            m_readyPromise->set_value();
        }
    }

    void subscribe(std::chrono::milliseconds timeout)
    {
        nx::cdb::PersistentParamsMap params;
        params["key1"] = "value1";
        params["key2"] = "value2";

        m_manager->subscribe(userTaskGuid, timeout, params);
    }

    void setShouldUnsubscribe() { m_shouldUnsubscribe = true; }
    int fired() const { return m_fired; }

private:
    nx::cdb::PersistentTimerManager* m_manager;
    nx::utils::promise* m_readyPromise;
    std::atomic<bool> m_shouldUnsubscribe = false;
    int m_fired = 0;
};

const QnUuid ManagerUser::userTaskGuid = QnUuid::fromStringSafe("{EC05F182-9380-48E3-9C76-AD6C10295136}");

TEST_F(PersistentTimerManager, subscribeUnsubscribe_simple)
{
    ManagerUser user(&manager, &readyPromise);
    manager.subscribe(std::chrono::milliseconds(10));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    readyFuture.wait();
    ASSERT_GE(manager.fired(), 0);
}

}
}
}
